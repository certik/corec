// Table of base_snprintf cases: X(expected, format, args...).
//
// Used by test_base.c (libc-free, runs on every platform including wasm) to
// check base_snprintf against `expected`, and by test_numconv_libc.c (hosted)
// to check that `expected` is what the host libc's snprintf produces.
// Only list cases whose behavior C defines and that base_snprintf supports
// exactly (see the limitations in base/numconv.h; e.g. use floats whose
// digits are exact so truncation and rounding agree).
#define NUMCONV_CASES(X) \
    /* plain conversions */ \
    X("42", "%d", 42) \
    X("-42", "%i", -42) \
    X("-2147483648", "%d", (int)(-2147483647 - 1)) \
    X("4294967295", "%u", 4294967295u) \
    X("deadbeef", "%x", 0xdeadbeefu) \
    X("DEADBEEF", "%X", 0xdeadbeefu) \
    X("777", "%o", 0777u) \
    X("0x1234", "%p", (void*)0x1234) \
    X("A", "%c", 'A') \
    X("hello", "%s", "hello") \
    X("100%", "100%%") \
    X("a=1 b=xy c=Z", "a=%d b=%s c=%c", 1, "xy", 'Z') \
    /* width, '-' and '0' with negative numbers */ \
    X("[  -42]", "[%5d]", -42) \
    X("[-42  ]", "[%-5d]", -42) \
    X("[-0042]", "[%05d]", -42) \
    X("[-42  ]", "[%-05d]", -42) \
    X("[-42]", "[%2d]", -42) \
    X("[   42]", "[%*d]", 5, 42) \
    X("[42   ]", "[%*d]", -5, 42) \
    X("[42   ]", "[%-*d]", 5, 42) \
    X("[  -1]", "[%4ld]", -1L) \
    X("[-00000000000000001]", "[%018lld]", -1LL) \
    /* '+' and ' ' */ \
    X("+5", "%+d", 5) \
    X(" 5", "% d", 5) \
    X("+5", "%+ d", 5) \
    X("-5", "% d", -5) \
    X("+0", "%+d", 0) \
    X("[  +42]", "[%+5d]", 42) \
    X("[+0042]", "[%+05d]", 42) \
    X("5", "%+u", 5u) \
    /* integer precision */ \
    X("007", "%.3d", 7) \
    X("-007", "%.3d", -7) \
    X("[-007 ]", "[%-5.3d]", -7) \
    X("[  007]", "[%05.3d]", 7) \
    X("", "%.0d", 0) \
    X("[     ]", "[%5.0d]", 0) \
    X("0", "%.*d", -1, 0) \
    X("00ff", "%.4x", 255u) \
    /* '#' */ \
    X("0xff", "%#x", 255u) \
    X("0XFF", "%#X", 255u) \
    X("0", "%#x", 0u) \
    X("0x0000ff", "%#08x", 255u) \
    X("010", "%#o", 8u) \
    X("0", "%#o", 0u) \
    X("0", "%#.0o", 0u) \
    X("00010", "%#.5o", 8u) \
    /* the LLVM IR string escape from certik/mlir#322 */ \
    X("\\0A", "\\%02X", 10) \
    X("\\FF", "\\%02X", 255) \
    /* length modifiers; hh and h convert the promoted int */ \
    X("-56", "%hhd", 200) \
    X("255", "%hhu", -1) \
    X("34", "%hhx", 0x1234) \
    X("4464", "%hd", 70000) \
    X("-1", "%hd", 65535) \
    X("65535", "%hu", -1) \
    X("ffff", "%hx", -1) \
    X("-9223372036854775808", "%lld", (long long)(-9223372036854775807LL - 1)) \
    X("18446744073709551615", "%llu", 18446744073709551615ULL) \
    X("FFFFFFFFFFFFFFFF", "%llX", 18446744073709551615ULL) \
    X("-2147483647", "%ld", -2147483647L) \
    X("12345", "%zu", (size_t)12345) \
    X("[   -5]", "[%5zd]", (size_t)-5) \
    X("-7", "%td", (ptrdiff_t)-7) \
    X("-3", "%jd", (long long)-3) \
    /* strings and chars */ \
    X("[   ab]", "[%5s]", "ab") \
    X("[ab   ]", "[%-5s]", "ab") \
    X("[a]", "[%.1s]", "abc") \
    X("[    a]", "[%5.1s]", "abc") \
    X("[abc]", "[%.*s]", 3, "abcdef") \
    X("[abcdef]", "[%.*s]", -1, "abcdef") \
    X("[  x]", "[%3c]", 'x') \
    X("[x  ]", "[%-3c]", 'x') \
    X("[    0x1234]", "[%10p]", (void*)0x1234) \
    X("[0x1234    ]", "[%-10p]", (void*)0x1234) \
    /* floating point */ \
    X("1.500000", "%f", 1.5) \
    X("-1.50", "%.2f", -1.5) \
    X("[   -1.500]", "[%9.3f]", -1.5) \
    X("[-1.500   ]", "[%-9.3f]", -1.5) \
    X("[-0001.50]", "[%08.2f]", -1.5) \
    X("+2.25", "%+.2f", 2.25) \
    X(" 2.25", "% .2f", 2.25) \
    X("-0.000000", "%f", -0.0) \
    X("-0", "%g", -0.0) \
    X("-1.25", "%g", -1.25) \
    X("[ 1.5]", "[%4g]", 1.5) \
    X("-1.500000e+00", "%e", -1.5) \
    X("[  1.23e+04]", "[%10.2e]", 12345.0) \
    X("+1.0e+00", "%+.1e", 1.0)
