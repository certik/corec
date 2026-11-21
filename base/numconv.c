#include <base/numconv.h>
#include <base/stdarg.h>

size_t uint64_to_str(uint64_t val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        return 1;
    }

    size_t len = 0;
    // Convert digits in reverse order
    while (val > 0) {
        buf[len++] = (char)('0' + (val % 10));
        val /= 10;
    }
    // Reverse
    for (size_t i = 0; i < len / 2; i++) {
        char t = buf[i];
        buf[i] = buf[len - 1 - i];
        buf[len - 1 - i] = t;
    }
    return len;
}

size_t int64_to_str(int64_t val, char* buf) {
    if (val < 0) {
        buf[0] = '-';
        size_t len = uint64_to_str(0 - (uint64_t)val, buf + 1);
        return len + 1;
    } else {
        return uint64_to_str((uint64_t)val, buf);
    }
}

size_t int_to_str(int val, char* buf) {
    return int64_to_str((int64_t)val, buf);
}

// Writes "[-]inf" / "[-]nan" and returns its length if `val` is not finite,
// otherwise returns 0.
static size_t nonfinite_to_str(double val, char* buf) {
    if (val - val == 0) return 0;  // finite
    size_t pos = 0;
    if (val < 0) buf[pos++] = '-';
    const char *s = val != val ? "nan" : "inf";
    for (int i = 0; i < 3; i++) buf[pos++] = s[i];
    return pos;
}

// Exact decimal digits of an integral `val` >= 2^63 (at most 309 digits):
// val = m * 2^e with e > 0, computed in base-1e9 limbs.
static size_t big_double_to_str(double val, char* buf) {
    union { double d; uint64_t u; } b;
    b.d = val;
    uint64_t m = (b.u & 0xFFFFFFFFFFFFFull) | ((uint64_t)1 << 52);
    int e = (int)(b.u >> 52 & 0x7ff) - 1075;
    uint32_t limb[36];
    int n = 0;
    while (m) { limb[n++] = (uint32_t)(m % 1000000000); m /= 1000000000; }
    while (e > 0) {
        int s = e < 29 ? e : 29;
        e -= s;
        uint64_t carry = 0;
        for (int i = 0; i < n; i++) {
            uint64_t x = ((uint64_t)limb[i] << s) + carry;
            limb[i] = (uint32_t)(x % 1000000000);
            carry = x / 1000000000;
        }
        while (carry) { limb[n++] = (uint32_t)(carry % 1000000000); carry /= 1000000000; }
    }
    size_t pos = uint64_to_str(limb[n - 1], buf);
    for (int i = n - 2; i >= 0; i--) {
        uint32_t v = limb[i];
        for (int k = 8; k >= 0; k--) { buf[pos + k] = (char)('0' + v % 10); v /= 10; }
        pos += 9;
    }
    return pos;
}

// "%f" of a finite `val` >= 0 with 0 <= `prec` <= 40. The integer part is
// exact. The fraction is rounded once (half up on the binary value) to
// min(prec, 17) digits in double arithmetic; any further digits are '0'.
static size_t fmt_f(double val, char* buf, int prec, int alt) {
    int p = prec < 17 ? prec : 17;
    uint64_t scale = 1;
    for (int i = 0; i < p; i++) scale *= 10;
    uint64_t frac = 0;
    size_t pos;
    if (val < 9223372036854775808.0) {  // 2^63
        uint64_t ip = (uint64_t)(int64_t)val;
        frac = (uint64_t)(int64_t)((val - (double)ip) * (double)scale + 0.5);
        // Values >= 2^53 have no fraction, so the carry stays below 2^63.
        if (frac >= scale) { frac -= scale; ip++; }
        pos = uint64_to_str(ip, buf);
    } else {
        // Integral already (>= 2^53); too large for int64_t.
        pos = big_double_to_str(val, buf);
    }
    if (prec > 0 || alt) buf[pos++] = '.';
    for (int i = p - 1; i >= 0; i--) { buf[pos + i] = (char)('0' + frac % 10); frac /= 10; }
    pos += p;
    for (int i = p; i < prec; i++) buf[pos++] = '0';
    return pos;
}

// Round a finite `val` >= 0 to `nd` (1..18) significant decimal digits,
// written to dig[0..nd); returns the decimal exponent X (val ~ d.ddd * 10^X).
// Uses repeated scaling by 10, so the digits are correct to about 15
// significant digits.
static int dec_digits(double val, char* dig, int nd) {
    int x = 0;
    if (val != 0.0) {
        while (val >= 10.0) { val /= 10.0; x++; }
        while (val < 1.0)   { val *= 10.0; x--; }
    }
    uint64_t scale = 1;
    for (int i = 1; i < nd; i++) scale *= 10;
    uint64_t m = (uint64_t)(int64_t)(val * (double)scale + 0.5);
    if (m >= scale * 10) { m /= 10; x++; }
    for (int i = nd - 1; i >= 0; i--) { dig[i] = (char)('0' + m % 10); m /= 10; }
    return x;
}

// "%e" (is_g == 0) or "%g" (is_g != 0) of a finite `val` >= 0 with `p` >= 1
// significant digits (p <= 341); digits past the 18th are '0'. %g uses fixed
// form unless X < -4 or X >= p, and drops trailing zeros (and a trailing '.')
// unless `alt`.
static size_t fmt_eg(double val, char* buf, int p, int alt, int is_g) {
    char dig[18];
    int nd = p < 18 ? p : 18;
    int x = dec_digits(val, dig, nd);
    int exp_form = !is_g || x < -4 || x >= p;
    int point = exp_form ? 0 : x;  // the '.' follows digit `point`
    size_t pos = 0;
    if (point < 0) {
        buf[pos++] = '0';
        buf[pos++] = '.';
        for (int k = -1; k > x; k--) buf[pos++] = '0';
    }
    for (int i = 0; i < p; i++) {
        buf[pos++] = i < nd ? dig[i] : '0';
        if (i == point && (i + 1 < p || alt)) buf[pos++] = '.';
    }
    if (is_g && !alt && (point < 0 || point + 1 < p)) {
        while (buf[pos - 1] == '0') pos--;
        if (buf[pos - 1] == '.') pos--;
    }
    if (exp_form) {
        buf[pos++] = 'e';
        buf[pos++] = x < 0 ? '-' : '+';
        if (x < 0) x = -x;
        if (x >= 100) buf[pos++] = (char)('0' + x / 100);
        buf[pos++] = (char)('0' + x / 10 % 10);
        buf[pos++] = (char)('0' + x % 10);
    }
    return pos;
}

// Sign and non-finite handling shared by the public wrappers below.
static size_t signed_float(double val, char* buf, int prec, int is_e) {
    size_t pos = nonfinite_to_str(val, buf);
    if (pos) return pos;
    if (val < 0) { buf[pos++] = '-'; val = -val; }
    if (prec < 0) prec = 6;
    if (prec > 40) prec = 40;
    return pos + (is_e ? fmt_eg(val, buf + pos, prec + 1, 0, 0) : fmt_f(val, buf + pos, prec, 0));
}

size_t double_to_str(double val, char* buf, int precision) {
    return signed_float(val, buf, precision, 0);
}

size_t double_to_str_e(double val, char* buf, int precision) {
    return signed_float(val, buf, precision, 1);
}

// vsnprintf/snprintf implementations (only for nostdlib builds)

// Output sink: counts every character, stores only those that fit (leaving
// room for the terminating NUL), as C's vsnprintf requires. The count stops
// at 2^31 (> INT_MAX), so it cannot wrap even with a 32-bit size_t.
typedef struct {
    char *s;
    size_t size;
    size_t n;
} fmt_out;

static void out_ch(fmt_out *o, char c) {
    if (o->n > 0x7fffffff) return;
    if (o->n + 1 < o->size) o->s[o->n] = c;
    o->n++;
}

static void out_rep(fmt_out *o, char c, size_t k) {
    while (k-- > 0 && o->n <= 0x7fffffff) out_ch(o, c);
}

// Parse a decimal width/precision, saturating at INT_MAX.
static int parse_num(const char **pp) {
    int v = 0;
    while (**pp >= '0' && **pp <= '9') {
        int d = *(*pp)++ - '0';
        v = v < 214748364 || (v == 214748364 && d <= 7) ? v * 10 + d : 0x7fffffff;
    }
    return v;
}

enum { F_LEFT = 1, F_ZERO = 2, F_PLUS = 4, F_SPACE = 8, F_ALT = 16 };

int base_vsnprintf(char *str, size_t size, const char *format, va_list args) {
    fmt_out o;
    o.s = str;
    o.size = size;
    o.n = 0;
    const char *p = format;
    // Fits the longest conversion: %f of DBL_MAX is 309 integer digits plus
    // '.' and at most 40 fractional digits.
    char buf[360];

    while (*p) {
        if (*p != '%' || !p[1]) {
            out_ch(&o, *p++);
            continue;
        }
        p++;

        // Flags.
        int flags = 0;
        for (;; p++) {
            if (*p == '-') flags |= F_LEFT;
            else if (*p == '0') flags |= F_ZERO;
            else if (*p == '+') flags |= F_PLUS;
            else if (*p == ' ') flags |= F_SPACE;
            else if (*p == '#') flags |= F_ALT;
            else break;
        }

        // Field width; a negative `*` width means left-justify.
        int width;
        if (*p == '*') {
            p++;
            width = va_arg(args, int);
            if (width < 0) {
                flags |= F_LEFT;
                width = width < -0x7fffffff ? 0x7fffffff : -width;
            }
        } else {
            width = parse_num(&p);
        }

        // Precision; a negative `*` precision is taken as omitted (-1).
        int prec = -1;
        if (*p == '.') {
            p++;
            if (*p == '*') {
                p++;
                prec = va_arg(args, int);
                if (prec < 0) prec = -1;
            } else {
                prec = parse_num(&p);
            }
        }

        // Length modifier: 1=hh 2=h 3=l 4=ll/j 5=z/t 6=L (intmax_t is 64-bit
        // and ptrdiff_t is size_t-sized on every target corec supports).
        int lmod = 0;
        if (*p == 'h') { p++; lmod = 2; if (*p == 'h') { p++; lmod = 1; } }
        else if (*p == 'l') { p++; lmod = 3; if (*p == 'l') { p++; lmod = 4; } }
        else if (*p == 'j') { p++; lmod = 4; }
        else if (*p == 'z' || *p == 't') { p++; lmod = 5; }
        else if (*p == 'L') { p++; lmod = 6; }

        char c = *p;
        if (!c) break;
        p++;

        if (flags & F_LEFT) flags &= ~F_ZERO;
        const char *pre = "";  // sign or radix prefix, emitted before zero padding
        size_t zeros = 0;      // zeros required by an integer precision
        char *body = buf;
        size_t blen = 0;
        // Lowercase digits at [0..16), uppercase at [16..32).
        const char *digits = "0123456789abcdef0123456789ABCDEF";

        switch (c) {
            case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': case 'p': {
                // Read the argument with its promoted type (by size), then
                // truncate (unsigned) or sign-extend (d/i) it to the
                // modifier's width.
                int bytes = c == 'p' ? (int)sizeof(void*)
                          : lmod == 4 ? 8
                          : lmod == 3 ? (int)sizeof(long)
                          : lmod == 5 ? (int)sizeof(size_t)
                          : (int)sizeof(int);
                uint64_t v = bytes == 8 ? va_arg(args, unsigned long long)
                                        : va_arg(args, unsigned int);
                int bits = lmod == 1 ? 8 : lmod == 2 ? 16 : bytes * 8;
                int is_signed = c == 'd' || c == 'i';
                if (bits < 64) {
                    uint64_t mask = ((uint64_t)1 << bits) - 1;
                    v &= mask;
                    if (is_signed && (v >> (bits - 1))) v |= ~mask;
                }
                if (is_signed) {
                    if ((int64_t)v < 0) { v = 0 - v; pre = "-"; }
                    else if (flags & F_PLUS) pre = "+";
                    else if (flags & F_SPACE) pre = " ";
                }
                if (v && (flags & F_ALT) && c == 'x') pre = "0x";
                if (v && (flags & F_ALT) && c == 'X') pre = "0X";
                if (c == 'p') pre = "0x";

                unsigned base = c == 'o' ? 8 : (c == 'u' || is_signed) ? 10 : 16;
                if (c == 'X') digits += 16;
                char *end = buf + sizeof(buf);
                body = end;
                while (v) { *--body = digits[v % base]; v /= base; }
                // A zero value prints "0", except with an explicit precision of 0
                // (but "%#.0o" still prints "0").
                if (body == end && (prec != 0 || ((flags & F_ALT) && c == 'o'))) *--body = '0';
                if ((flags & F_ALT) && c == 'o' && *body != '0') *--body = '0';
                blen = (size_t)(end - body);
                if (prec >= 0) {
                    flags &= ~F_ZERO;
                    if ((size_t)prec > blen) zeros = (size_t)prec - blen;
                }
                break;
            }
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
            case 'a': case 'A': {
                if (lmod == 6 || c == 'a' || c == 'A') {
                    // long double and hex floats are not supported: consume
                    // the argument, print nothing.
#ifdef __TINYC__
                    (void)va_arg(args, double);  // tinyC has no long double
#else
                    if (lmod == 6) (void)va_arg(args, long double);
                    else (void)va_arg(args, double);
#endif
                    continue;
                }
                double val = va_arg(args, double);
                // Take the sign from the sign bit so that -0.0 prints as "-0".
                union { double d; uint64_t u; } fb;
                fb.d = val;
                if (fb.u >> 63) { val = -val; pre = "-"; }
                else if (flags & F_PLUS) pre = "+";
                else if (flags & F_SPACE) pre = " ";
                char lc = (char)(c | 0x20);  // lowercase conversion
                int alt = flags & F_ALT;
                if (val - val != 0) {
                    blen = nonfinite_to_str(val, buf);
                    flags &= ~F_ZERO;
                } else if (lc == 'g') {
                    // C: precision 0 means 1. Capping at 340 keeps the output
                    // within buf and cannot change the style (X <= 308).
                    int p = prec < 0 ? 6 : prec == 0 ? 1 : prec > 340 ? 340 : prec;
                    blen = fmt_eg(val, buf, p, alt, 1);
                } else {
                    int p = prec < 0 ? 6 : prec > 40 ? 40 : prec;  // keep within buf
                    blen = lc == 'e' ? fmt_eg(val, buf, p + 1, alt, 0) : fmt_f(val, buf, p, alt);
                }
                if (c != lc) {
                    for (size_t i = 0; i < blen; i++)
                        if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] = (char)(buf[i] - 32);
                }
                break;
            }
            case 's': {
                body = va_arg(args, char*);
                if (body == NULL) body = "(null)";
                // With a precision, read at most `prec` chars: the string need
                // not be NUL-terminated (e.g. corec `string` slices via %.*s).
                while ((prec < 0 || blen < (size_t)prec) && body[blen]) blen++;
                flags &= ~F_ZERO;
                break;
            }
            case 'c':
                buf[0] = (char)va_arg(args, int);
                blen = 1;
                flags &= ~F_ZERO;
                break;
            case '%':
                buf[0] = '%';
                blen = 1;
                width = 0;
                break;
            case 'n':
                // Writing the count through the pointer is not supported;
                // consume the argument so later conversions stay aligned.
                (void)va_arg(args, void*);
                continue;
            default:
                // Unknown conversion: skip it.
                continue;
        }

        // [spaces][pre][zero padding][precision zeros][body][spaces]
        size_t plen = 0;
        while (pre[plen]) plen++;
        size_t used = plen + zeros + blen;
        size_t pad = (size_t)width > used ? (size_t)width - used : 0;
        if (!(flags & (F_LEFT | F_ZERO))) out_rep(&o, ' ', pad);
        for (size_t i = 0; i < plen; i++) out_ch(&o, pre[i]);
        if (flags & F_ZERO) out_rep(&o, '0', pad);
        out_rep(&o, '0', zeros);
        for (size_t i = 0; i < blen; i++) out_ch(&o, body[i]);
        if (flags & F_LEFT) out_rep(&o, ' ', pad);
    }

    if (size > 0) str[o.n < size ? o.n : size - 1] = '\0';
    // C returns a negative value (EOVERFLOW) if the length exceeds INT_MAX.
    return o.n > 0x7fffffff ? -1 : (int)o.n;
}

int base_snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = base_vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}
