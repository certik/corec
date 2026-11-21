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
        size_t len = uint64_to_str((uint64_t)(-val), buf + 1);
        return len + 1;
    } else {
        return uint64_to_str((uint64_t)val, buf);
    }
}

size_t int_to_str(int val, char* buf) {
    return int64_to_str((int64_t)val, buf);
}

size_t double_to_str(double val, char* buf, int precision) {
    // Simple implementation: handle sign, integer part, decimal point, fractional part
    size_t pos = 0;

    if (val < 0) {
        buf[pos++] = '-';
        val = -val;
    }

    // Integer part
    int64_t int_part = (int64_t)val;
    pos += int64_to_str(int_part, buf + pos);

    // Default precision is 6 if not specified
    if (precision < 0) precision = 6;

    if (precision > 0) {
        buf[pos++] = '.';

        // Fractional part
        double frac_part = val - (double)int_part;
        for (int i = 0; i < precision; i++) {
            frac_part *= 10;
            int digit = (int)frac_part;
            buf[pos++] = '0' + digit;
            frac_part -= digit;
        }
    }

    return pos;
}

static size_t double_to_str_g(double val, char* buf, int precision) {
    if (precision < 0) precision = 6;
    size_t len = double_to_str(val, buf, precision);
    while (len > 0 && buf[len - 1] == '0') len--;
    if (len > 0 && buf[len - 1] == '.') len--;
    if (len == 0) buf[len++] = '0';
    return len;
}

// Format `val` as `[-]d.ddde[+-]NN` (C's "%e"), with `precision` fractional
// digits in the mantissa. Always at least two exponent digits.
size_t double_to_str_e(double val, char* buf, int precision) {
    size_t pos = 0;

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

size_t uint64_to_hex_str(uint64_t val, char* buf, int uppercase) {
    if (val == 0) {
        buf[0] = '0';
        return 1;
    }

    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t len = 0;
    
    // Convert digits in reverse order
    while (val > 0) {
        buf[len++] = digits[val & 0xF];
        val >>= 4;
    }
    
    // Reverse
    for (size_t i = 0; i < len / 2; i++) {
        char t = buf[i];
        buf[i] = buf[len - 1 - i];
        buf[len - 1 - i] = t;
    }
    return len;
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

static void out_rep(fmt_out *o, char c, int k) {
    while (k-- > 0) out_ch(o, c);
}

enum { F_LEFT = 1, F_ZERO = 2, F_PLUS = 4, F_SPACE = 8, F_ALT = 16 };

int base_vsnprintf(char *str, size_t size, const char *format, va_list args) {
    fmt_out o;
    o.s = str;
    o.size = size;
    o.n = 0;
    const char *p = format;
    char buf[64];

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
        int width = 0;
        if (*p == '*') {
            p++;
            width = va_arg(args, int);
            if (width < 0) { flags |= F_LEFT; width = -width; }
        } else {
            while (*p >= '0' && *p <= '9') width = width * 10 + (*p++ - '0');
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
                prec = 0;
                while (*p >= '0' && *p <= '9') prec = prec * 10 + (*p++ - '0');
            }
        }

        // Length modifier: 1=hh 2=h 3=l 4=ll/j 5=z/t (intmax_t is 64-bit and
        // ptrdiff_t is size_t-sized on every target corec supports).
        int lmod = 0;
        if (*p == 'h') { p++; lmod = 2; if (*p == 'h') { p++; lmod = 1; } }
        else if (*p == 'l') { p++; lmod = 3; if (*p == 'l') { p++; lmod = 4; } }
        else if (*p == 'j') { p++; lmod = 4; }
        else if (*p == 'z' || *p == 't') { p++; lmod = 5; }

        char c = *p;
        if (!c) break;
        p++;

        if (flags & F_LEFT) flags &= ~F_ZERO;
        const char *pre = "";  // sign or radix prefix, emitted before zero padding
        int zeros = 0;         // zeros required by an integer precision
        char *body = buf;
        size_t blen = 0;

        switch (c) {
            case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': case 'p': {
                // Read the argument with its promoted type, then truncate
                // (unsigned) or sign-extend (d/i) it to the modifier's width.
                uint64_t v;
                int bits = 64;
                if (c == 'p') {
                    v = (uint64_t)(uintptr_t)va_arg(args, void*);
                } else if (lmod == 4) {
                    v = va_arg(args, unsigned long long);
                } else if (lmod == 3) {
                    v = va_arg(args, unsigned long);
                    bits = (int)sizeof(long) * 8;
                } else if (lmod == 5) {
                    v = va_arg(args, size_t);
                    bits = (int)sizeof(size_t) * 8;
                } else {
                    v = va_arg(args, unsigned int);
                    bits = lmod == 1 ? 8 : lmod == 2 ? 16 : (int)sizeof(int) * 8;
                }
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
                const char *digits = c == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
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
                    if ((size_t)prec > blen) zeros = prec - (int)blen;
                }
                break;
            }
            case 'f': case 'e': case 'g': {
                double val = va_arg(args, double);
                // Take the sign from the sign bit so that -0.0 prints as "-0".
                union { double d; uint64_t u; } fb;
                fb.d = val;
                if (fb.u >> 63) { val = -val; pre = "-"; }
                else if (flags & F_PLUS) pre = "+";
                else if (flags & F_SPACE) pre = " ";
                if (prec > 40) prec = 40;  // keep within buf
                if ((fb.u >> 52 & 0x7ff) == 0x7ff) {
                    body = (fb.u << 12) ? "nan" : "inf";
                    blen = 3;
                    flags &= ~F_ZERO;
                } else if (c == 'g') blen = double_to_str_g(val, buf, prec);
                else if (c == 'e') blen = double_to_str_e(val, buf, prec);
                else blen = double_to_str(val, buf, prec);
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
            default:
                // Unknown conversion: skip it.
                continue;
        }

        // [spaces][pre][zero padding][precision zeros][body][spaces]
        int plen = 0;
        while (pre[plen]) plen++;
        int pad = width - plen - zeros - (int)blen;
        if (!(flags & (F_LEFT | F_ZERO))) out_rep(&o, ' ', pad);
        for (int i = 0; i < plen; i++) out_ch(&o, pre[i]);
        if (flags & F_ZERO) out_rep(&o, '0', pad);
        out_rep(&o, '0', zeros);
        for (size_t i = 0; i < blen; i++) out_ch(&o, body[i]);
        if (flags & F_LEFT) out_rep(&o, ' ', pad);
    }

    if (size > 0) str[o.n < size ? o.n : size - 1] = '\0';
    return (int)o.n;
}

int base_snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = base_vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}
