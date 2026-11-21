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

// "%.<precision>f": the integer part is exact; the fractional digits are
// truncated, not rounded.
size_t double_to_str(double val, char* buf, int precision) {
    size_t pos = nonfinite_to_str(val, buf);
    if (pos) return pos;

    if (val < 0) {
        buf[pos++] = '-';
        val = -val;
    }

    double frac_part = 0;
    if (val < 9223372036854775808.0) {  // 2^63
        int64_t int_part = (int64_t)val;
        pos += uint64_to_str((uint64_t)int_part, buf + pos);
        frac_part = val - (double)int_part;
    } else {
        // Integral already (>= 2^53); too large for int64_t.
        pos += big_double_to_str(val, buf + pos);
    }

    // Default precision is 6 if not specified
    if (precision < 0) precision = 6;

    if (precision > 0) {
        buf[pos++] = '.';
        for (int i = 0; i < precision; i++) {
            frac_part *= 10;
            int digit = (int)frac_part;
            if (digit > 9) digit = 9;
            buf[pos++] = (char)('0' + digit);
            frac_part -= digit;
        }
    }

    return pos;
}

// Format `val` as `[-]d.ddde[+-]NN` (C's "%e"), with `precision` fractional
// digits in the mantissa. Always at least two exponent digits.
size_t double_to_str_e(double val, char* buf, int precision) {
    size_t pos = nonfinite_to_str(val, buf);
    if (pos) return pos;

    int neg = 0;
    if (val < 0) { neg = 1; val = -val; }
    if (precision < 0) precision = 6;
    // Clamp precision so the integer mantissa fits in int64_t.
    if (precision > 17) precision = 17;

    // Normalize to [1, 10) and compute the decimal exponent.
    int exp = 0;
    if (val != 0.0) {
        while (val >= 10.0) { val /= 10.0; exp++; }
        while (val < 1.0)   { val *= 10.0; exp--; }
    }

    // Compute the integer mantissa with `precision+1` digits, rounded.
    double scale = 1.0;
    for (int i = 0; i < precision; i++) scale *= 10.0;
    int64_t mantissa = (int64_t)(val * scale + 0.5);
    int64_t cutoff = 10;
    for (int i = 0; i < precision; i++) cutoff *= 10;
    if (mantissa >= cutoff) { mantissa /= 10; exp++; }

    if (neg) buf[pos++] = '-';

    // First digit (integer part).
    int64_t scale_i = (int64_t)scale;
    int first = (int)(mantissa / scale_i);
    int64_t frac = mantissa % scale_i;
    if (first > 9) first = 9;
    buf[pos++] = (char)('0' + first);

    if (precision > 0) {
        buf[pos++] = '.';
        char digbuf[32];
        int n = 0;
        int64_t r = frac;
        while (r > 0) { digbuf[n++] = (char)('0' + (r % 10)); r /= 10; }
        while (n < precision) digbuf[n++] = '0';
        while (n > 0) buf[pos++] = digbuf[--n];
    }

    buf[pos++] = 'e';
    if (exp < 0) { buf[pos++] = '-'; exp = -exp; }
    else         { buf[pos++] = '+'; }

    char digits[8];
    int n = 0;
    if (exp == 0) digits[n++] = '0';
    while (exp > 0) { digits[n++] = (char)('0' + (exp % 10)); exp /= 10; }
    while (n < 2) digits[n++] = '0';
    while (n > 0) buf[pos++] = digits[--n];

    return pos;
}

// C's "%g" for a finite `val` >= 0: `prec` significant digits (at most 18),
// taken from "%e"; exponent form if the exponent X < -4 or X >= prec, fixed
// otherwise; trailing zeros (and a trailing '.') removed unless `alt`.
static size_t double_to_str_g(double val, char* buf, int prec, int alt) {
    char e[32];
    if (prec < 0) prec = 6;
    if (prec == 0) prec = 1;
    if (prec > 18) prec = 18;
    size_t n = double_to_str_e(val, e, prec - 1);
    size_t m = 0;  // mantissa "d[.ddd]" is e[0..m)
    while (e[m] != 'e') m++;
    int x = 0;
    for (size_t i = m + 2; i < n; i++) x = x * 10 + (e[i] - '0');
    if (e[m + 1] == '-') x = -x;

    size_t pos = 0;
    int exp_form = x < -4 || x >= prec;
    if (exp_form) {
        for (size_t i = 0; i < m; i++) buf[pos++] = e[i];
    } else {
        if (x < 0) {
            buf[pos++] = '0';
            buf[pos++] = '.';
            for (int k = -1; k > x; k--) buf[pos++] = '0';
        }
        // The digits are e[0] and e[2..m); the point goes after digit x.
        for (size_t i = 0; i < m; i++) {
            if (i == 1) continue;
            buf[pos++] = e[i];
            if (x >= 0 && (size_t)x == (i == 0 ? 0 : i - 1) && i + 1 < m) buf[pos++] = '.';
        }
    }
    int has_dot = 0;
    for (size_t i = 0; i < pos; i++) if (buf[i] == '.') has_dot = 1;
    if (alt && !has_dot) buf[pos++] = '.';
    if (!alt && has_dot) {
        while (buf[pos - 1] == '0') pos--;
        if (buf[pos - 1] == '.') pos--;
    }
    if (exp_form) for (size_t i = m; i < n; i++) buf[pos++] = e[i];
    return pos;
}

// vsnprintf/snprintf implementations (only for nostdlib builds)

// Output sink: counts every character, stores only those that fit (leaving
// room for the terminating NUL), as C's vsnprintf requires.
typedef struct {
    char *s;
    size_t size;
    size_t n;
} fmt_out;

static void out_ch(fmt_out *o, char c) {
    if (o->n + 1 < o->size) o->s[o->n] = c;
    o->n++;
}

static void out_rep(fmt_out *o, char c, size_t k) {
    while (k-- > 0) out_ch(o, c);
}

// Parse a decimal width/precision, saturating at INT_MAX.
static int parse_num(const char **pp) {
    int v = 0;
    while (**pp >= '0' && **pp <= '9') {
        int d = *(*pp)++ - '0';
        v = v <= (0x7fffffff - 9) / 10 ? v * 10 + d : 0x7fffffff;
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
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
                if (lmod == 6) {
                    // long double is not supported: consume it, print nothing.
#ifdef __TINYC__
                    (void)va_arg(args, double);  // tinyC has no long double
#else
                    (void)va_arg(args, long double);
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
                if (val - val != 0) {
                    blen = nonfinite_to_str(val, buf);
                    flags &= ~F_ZERO;
                } else if (lc == 'g') {
                    blen = double_to_str_g(val, buf, prec, flags & F_ALT);
                } else {
                    if (prec > 40) prec = 40;  // keep within buf
                    if (lc == 'e') blen = double_to_str_e(val, buf, prec);
                    else blen = double_to_str(val, buf, prec);
                    if (prec == 0 && (flags & F_ALT)) {
                        // '#': always print the decimal point.
                        size_t at = lc == 'e' ? 1 : blen;
                        for (size_t i = blen; i > at; i--) buf[i] = buf[i - 1];
                        buf[at] = '.';
                        blen++;
                    }
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
            case 'a': case 'A':
                // Hex floats are not supported: consume the argument, print
                // nothing.
                if (lmod == 6) {
#ifdef __TINYC__
                    (void)va_arg(args, double);
#else
                    (void)va_arg(args, long double);
#endif
                } else {
                    (void)va_arg(args, double);
                }
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
