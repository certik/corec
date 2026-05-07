#!/usr/bin/env bash
# Build corec_test_macos_tinyc by compiling each corec C source with tinyC
# (the small MLIR-based C compiler at ../mlir/examples/tinyc) and linking
# the resulting .ll files with clang.
#
# tinyC's preprocessor is intentionally tiny. Real corec headers exercise
# every dark corner of the C preprocessor (COUNT_ARGS / FOR_EACH style
# variadic macros, __VA_OPT__, _Pragma, __attribute__ pass-through, ...),
# which is genuinely freestanding-C territory. So we delegate preprocessing
# to system clang: `clang -E -nostdinc -fno-builtin -I platform -I .` per
# .c file, then feed the preprocessed output to tinyC. Only the *compile*
# (parse → MLIR → LLVM IR) step uses tinyC.

set -euo pipefail

cd "$(dirname "$0")/.."

TINYC="${TINYC:-../mlir/tinyc}"
if [ ! -x "$TINYC" ]; then
    echo "tinyC binary not found at $TINYC." >&2
    echo "Build it first:  cd ../mlir && pixi run -e upstream build_tinyc_upstream" >&2
    exit 1
fi

OUT=corec_test_macos_tinyc
WORK=build_tinyc
rm -rf "$WORK"
mkdir -p "$WORK"

SOURCES=(
    test_base_only.c
    test_base.c
    base/io.c
    base/buddy.c
    base/arena.c
    base/scratch.c
    base/format.c
    base/math.c
    base/string.c
    base/mem.c
    base/numconv.c
    base/assert.c
    base/exit.c
    platform/platform_macos.c
)

LL_FILES=()
for src in "${SOURCES[@]}"; do
    pp="$WORK/$(echo "$src" | tr '/' '_').i"
    ll="$WORK/$(echo "$src" | tr '/' '_').ll"
    echo "[clang -E] $src"
    clang -E -P -nostdinc -fno-builtin -DNDEBUG -I platform -I . "$src" -o "$pp"
    echo "[tinyc   ] $pp -> $ll"
    "$TINYC" --emit=llvm -o "$ll" "$pp"
    LL_FILES+=("$ll")
done

echo "[link    ] $OUT"
clang -nostdlib -fno-builtin -o "$OUT" "${LL_FILES[@]}" -lSystem -Wl,-e,__start
echo "Built $OUT"
