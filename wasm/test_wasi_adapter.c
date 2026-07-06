// Native unit test for wasm/wasi_adapter.c (the wasm->native lift host shim).
//
// The adapter defines the WASI preview1 imports (fd_write, fd_read, path_open,
// args_*, ...) in terms of i32 linear-memory offsets, translates those offsets
// to host pointers via __wasm_linmem_base, and delegates the real I/O to the
// corec platform API. In the real lift, tinyC compiles the adapter and the host
// platform_*.c with `-Dplatform_<fn>=__host_platform_<fn>` so the host copies do
// not collide with the wasm-side platform_* baked into the lifted module.
//
// This test reproduces that exact link WITHOUT tinyC: it points
// __wasm_linmem_base at a plain host buffer that plays the role of the wasm
// linear memory, drives the adapter's WASI entry points against offsets into
// that buffer, and links the real platform_{linux,macos}.c (compiled with the
// same renames) underneath. It therefore exercises the offset-translation +
// delegation logic that would otherwise silently drift from platform.h.
//
// This is a host-side test harness, so (unlike the corec library itself) it is
// allowed to use libc.

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "wasi_adapter.h"

// Globals the adapter reads (normally set by the lifted binary's crt0/_start).
u64 __wasm_linmem_base;
i32 __wasm_argc;
u64 __wasm_argv;

// platform_init() (unused here) references buddy_init(); stub it so this
// I/O-only test does not have to link base/buddy.c.
void buddy_init(void) {}

// platform.h constants, duplicated here so the harness needs no corec headers.
#define RIGHT_FD_READ  0x2
#define RIGHT_FD_WRITE 0x40
#define O_CREAT_       0x1
#define O_TRUNC_       0x8
#define SEEK_SET_      0

static unsigned char LM[1 << 16];
static u32  wr32(u32 off, u32 v) { *(u32 *)(LM + off) = v; return off; }
static u32  rd32(u32 off)        { return *(u32 *)(LM + off); }
static void put(u32 off, const char *s) { memcpy(LM + off, s, strlen(s)); }

static int failures = 0;
#define CHECK(cond, msg) do {                                   \
    if (cond) { printf("ok   - %s\n", (msg)); }                 \
    else      { printf("FAIL - %s\n", (msg)); failures++; }     \
} while (0)

int main(void) {
    __wasm_linmem_base = (u64)(unsigned long)LM;

    const char *path = "wasi_adapter_test.tmp";
    const char *msg  = "hello wasi\n";              // 11 bytes
    const u32   path_off = 256, fd_out = 512, buf_off = 1024, iov = 600, nw = 700;

    // --- path_open (create+trunc, write) -> fd_write -> fd_close ---
    put(path_off, path);
    u32 rc = path_open(3, 0, path_off, (u32)strlen(path),
                       O_CREAT_ | O_TRUNC_, RIGHT_FD_WRITE, 0, 0, fd_out);
    CHECK(rc == 0, "path_open create");
    u32 wfd = rd32(fd_out);
    CHECK(wfd >= 3, "created fd is >= 3 (does not shadow std streams)");

    {
        struct stat st;
        CHECK(stat(path, &st) == 0, "stat created file");
        CHECK((st.st_mode & 0777) == 0644, "O_CREAT file mode is 0644");
    }

    put(buf_off, msg);
    wr32(iov, buf_off);
    wr32(iov + 4, (u32)strlen(msg));
    rc = fd_write(wfd, iov, 1, nw);
    CHECK(rc == 0 && rd32(nw) == (u32)strlen(msg), "fd_write reports full byte count");
    fd_close(wfd);

    // --- path_open (read) -> fd_seek/fd_tell -> fd_read -> compare ---
    rc = path_open(3, 0, path_off, (u32)strlen(path), 0, RIGHT_FD_READ, 0, 0, fd_out);
    CHECK(rc == 0, "path_open read");
    u32 rfd = rd32(fd_out);

    const u32 newoff = 800, telloff = 808, rbuf = 2048, riov = 900, nr = 908;
    rc = fd_seek(rfd, 6, SEEK_SET_, newoff);
    CHECK(rc == 0 && *(u64 *)(LM + newoff) == 6, "fd_seek to offset 6");
    rc = fd_tell(rfd, telloff);
    CHECK(rc == 0 && *(u64 *)(LM + telloff) == 6, "fd_tell reports offset 6");

    wr32(riov, rbuf);
    wr32(riov + 4, 64);
    rc = fd_read(rfd, riov, 1, nr);
    CHECK(rc == 0 && rd32(nr) == 5, "fd_read returns the 5 bytes after the seek");
    CHECK(memcmp(LM + rbuf, "wasi\n", 5) == 0, "fd_read content matches");
    fd_close(rfd);

    // --- args_sizes_get / args_get ---
    char *av[3] = { "prog", "a1", "bb2" };
    __wasm_argc = 3;
    __wasm_argv = (u64)(unsigned long)av;

    const u32 oc = 1000, obs = 1004, ofs_arr = 1100, abuf = 1200;
    args_sizes_get(oc, obs);
    CHECK(rd32(oc) == 3, "args_sizes_get argc");
    CHECK(rd32(obs) == (u32)(5 + 3 + 4), "args_sizes_get buffer size (incl nuls)");
    args_get(ofs_arr, abuf);
    CHECK(strcmp((char *)(LM + rd32(ofs_arr + 0 * 4)), "prog") == 0, "args_get argv[0]");
    CHECK(strcmp((char *)(LM + rd32(ofs_arr + 1 * 4)), "a1")   == 0, "args_get argv[1]");
    CHECK(strcmp((char *)(LM + rd32(ofs_arr + 2 * 4)), "bb2")  == 0, "args_get argv[2]");

    // --- fd_write straight to the real stdout, through the adapter ---
    const u32 sbuf = 3000, siov = 3100, snw = 3200;
    const char *banner = "[wasi_adapter] stdout write via fd_write ok\n";
    put(sbuf, banner);
    wr32(siov, sbuf);
    wr32(siov + 4, (u32)strlen(banner));
    fd_write(1, siov, 1, snw);

    printf(failures ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", failures);
    return failures ? 1 : 0;
}
