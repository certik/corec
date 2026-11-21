// Differential test of base_vsnprintf against the host libc's vsnprintf.
//
// Unlike the rest of the tests this one is built *with* libc (it needs a
// reference snprintf); only base/numconv.c is linked from corec. It checks:
//   1. every case in test_numconv_cases.h: the expected string is what libc
//      produces, and base_vsnprintf matches libc for every buffer size from
//      0 (with str == NULL) up to the full length, including the return value
//      and that nothing is written past `size`;
//   2. a sweep over flags x width x precision x length modifier x conversion
//      for a set of integer, string, char and float values.

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "test_numconv_cases.h"

int base_vsnprintf(char *str, size_t size, const char *format, va_list args);

static int failures = 0;
static int checks = 0;

#define BUF 512

static void check_v(const char *expected, const char *fmt, va_list ap) {
    char ref[BUF], got[BUF];
    va_list a;
    va_copy(a, ap);
    int nref = vsnprintf(ref, sizeof(ref), fmt, a);
    va_end(a);
    checks++;
    if (nref < 0 || nref >= BUF) {
        printf("FAIL: libc vsnprintf(\"%s\") returned %d\n", fmt, nref);
        failures++;
        return;
    }
    if (expected && strcmp(expected, ref) != 0) {
        printf("FAIL: table expects \"%s\" for \"%s\", libc gives \"%s\"\n",
               expected, fmt, ref);
        failures++;
    }
    // size == 0 with str == NULL just measures.
    va_copy(a, ap);
    int n = base_vsnprintf(NULL, 0, fmt, a);
    va_end(a);
    if (n != nref) {
        printf("FAIL: \"%s\" (NULL, 0): base returned %d, libc %d\n", fmt, n, nref);
        failures++;
    }
    for (size_t size = 0; size <= (size_t)nref + 1; size++) {
        memset(got, '#', sizeof(got));
        va_copy(a, ap);
        n = base_vsnprintf(got, size, fmt, a);
        va_end(a);
        size_t keep = size == 0 ? 0 : ((size_t)nref < size ? (size_t)nref : size - 1);
        int ok = n == nref && memcmp(got, ref, keep) == 0 &&
                 (size == 0 || got[keep] == '\0');
        for (size_t i = size; i < sizeof(got); i++) {
            if (got[i] != '#') ok = 0;
        }
        if (!ok) {
            printf("FAIL: \"%s\" size=%zu: base returned %d \"%.*s\", libc %d \"%s\"\n",
                   fmt, size, n, (int)keep, got, nref, ref);
            failures++;
            return;
        }
    }
}

static void check(const char *expected, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    check_v(expected, fmt, ap);
    va_end(ap);
}

static void table_cases(void) {
#define X(expected, ...) check(expected, __VA_ARGS__);
    NUMCONV_CASES(X)
#undef X
}

// Build "%<flags><width><prec><lmod><conv>" from the pieces.
static const char *mkfmt(char *out, unsigned flagset, const char *width,
                         const char *prec, const char *lmod, char conv) {
    static const char flagchars[] = "-0+ #";
    char *p = out;
    *p++ = '[';
    *p++ = '%';
    for (int i = 0; i < 5; i++) {
        if (flagset & (1u << i)) *p++ = flagchars[i];
    }
    p += sprintf(p, "%s%s%s%c]", width, prec, lmod, conv);
    return out;
}

static const char *widths[] = {"", "1", "4", "12", "*"};
static const char *precs[] = {"", ".", ".0", ".1", ".3", ".12", ".*"};

static void sweep_ints(void) {
    static const long long vals[] = {
        0, 1, -1, 7, -7, 42, -42, 127, 128, 255, 256, -128, -129, 32767,
        32768, 65535, 65536, -32769, 2147483647LL, -2147483647LL - 1,
        4294967295LL, 4294967296LL, 0x123456789abcdefLL,
        9223372036854775807LL, -9223372036854775807LL - 1,
    };
    static const char *lmods[] = {"", "hh", "h", "l", "ll", "z", "j", "t"};
    static const char convs[] = "diuoxX";
    char fmt[64];
    for (size_t li = 0; li < sizeof(lmods) / sizeof(lmods[0]); li++)
    for (size_t ci = 0; convs[ci]; ci++)
    for (unsigned fl = 0; fl < 32; fl++)
    for (size_t wi = 0; wi < sizeof(widths) / sizeof(widths[0]); wi++)
    for (size_t pi = 0; pi < sizeof(precs) / sizeof(precs[0]); pi++) {
        char conv = convs[ci];
        // '#' is only defined for o, x and X.
        if ((fl & 16) && (conv == 'd' || conv == 'i' || conv == 'u')) continue;
        const char *l = lmods[li];
        mkfmt(fmt, fl, widths[wi], precs[pi], l, conv);
        int wstar = widths[wi][0] == '*', pstar = precs[pi][0] && precs[pi][1] == '*';
        for (int sw = -1; sw <= 1; sw += 2)
        for (size_t vi = 0; vi < sizeof(vals) / sizeof(vals[0]); vi++) {
            if (!wstar && sw > 0) continue;
            int w = 7 * sw, pr = sw > 0 ? 5 : -1;
            long long v = vals[vi];
            // Pass the argument with the type the length modifier expects.
#define CALL(arg) \
            do { \
                if (wstar && pstar) check(NULL, fmt, w, pr, arg); \
                else if (wstar) check(NULL, fmt, w, arg); \
                else if (pstar) check(NULL, fmt, pr, arg); \
                else check(NULL, fmt, arg); \
            } while (0)
            if (l[0] == 'l' && l[1] == 'l') CALL(v);
            else if (l[0] == 'j') CALL((long long)v);
            else if (l[0] == 'l') CALL((long)v);
            else if (l[0] == 'z') CALL((size_t)v);
            else if (l[0] == 't') CALL((ptrdiff_t)v);
            else CALL((int)v);
#undef CALL
        }
    }
}

static void sweep_other(void) {
    static const char *strs[] = {"", "a", "hello", "hello, world"};
    static const char chars[] = {'x', ' ', '%'};
    // base %f truncates its fractional digits (C rounds), and base %e/%g round
    // exact decimal ties up (C rounds them to even), so only use values and
    // precisions where neither matters.
    static const double dbls[] = {0.0, -0.0, 0.5, -0.5, 1.5, -2.25, 100.75, 12345.0,
                                  123456789.0, 0.000123456, 1e-10, 1e19, -1e20, 1e300};
    static const char *dfmts[] = {"f", ".2f", ".3f", "F", "e", ".0e", ".2e", "E",
                                  "g", ".0g", ".1g", ".3g", "G"};
    char fmt[64];
    for (unsigned fl = 0; fl < 32; fl++)
    for (size_t wi = 0; wi < sizeof(widths) / sizeof(widths[0]) - 1; wi++) {
        // '0' and '#' are only defined for numeric conversions.
        if (!(fl & (2 | 16))) {
            for (size_t pi = 0; pi < sizeof(precs) / sizeof(precs[0]) - 1; pi++)
            for (size_t si = 0; si < sizeof(strs) / sizeof(strs[0]); si++)
                check(NULL, mkfmt(fmt, fl, widths[wi], precs[pi], "", 's'), strs[si]);
            for (size_t ci = 0; ci < sizeof(chars); ci++)
                check(NULL, mkfmt(fmt, fl, widths[wi], "", "", 'c'), chars[ci]);
            // '+' and ' ' are only defined for signed conversions.
            if (!(fl & (4 | 8)))
                check(NULL, mkfmt(fmt, fl, widths[wi], "", "", 'p'), (void*)0xabc);
        }
        for (size_t fi = 0; fi < sizeof(dfmts) / sizeof(dfmts[0]); fi++) {
            const char *d = dfmts[fi];
            char prec[8] = "";
            if (d[0] == '.') { prec[0] = '.'; prec[1] = d[1]; prec[2] = 0; d += 2; }
            for (size_t di = 0; di < sizeof(dbls) / sizeof(dbls[0]); di++)
                check(NULL, mkfmt(fmt, fl, widths[wi], prec, "", d[0]), dbls[di]);
        }
    }
}

int main(void) {
    table_cases();
    int table = checks;
    sweep_ints();
    sweep_other();
    printf("numconv libc test: %d table cases, %d sweep cases, %d failures\n",
           table, checks - table, failures);
    return failures != 0;
}
