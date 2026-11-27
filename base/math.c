#include <base/types.h>
#include <base/math.h>

// =============================================================================
// Fast math functions (argument reduction + Estrin polynomial approximation)
//
// Design goals:
//   - High performance (no libm dependency, minimal branching, Estrin ILP).
//   - Double precision: >= 1e-14 relative accuracy on the supported range.
//   - Single precision: < 1e-6 relative accuracy on the supported range.
//   - Portable C11 compatible with clang, GCC, MSVC, and tinyC.
// =============================================================================

// 2 / pi
static const double INV_PI2_D = 0.63661977236758134307553505349006;

// Cody-Waite split of pi/2 = PI2_HI + PI2_MID + PI2_LO.
// PI2_HI and PI2_MID each have 20 trailing zero mantissa bits so k * PI2_HI
// and k * PI2_MID are exact in IEEE-754 double for |k| <= 2^20 (|x| <= ~1.6e6).
static const double PI2_HI  = 1.57079632673412561e+00;
static const double PI2_MID = 6.07710050630396598e-11;
static const double PI2_LO  = 2.02226624879595063e-21;

// ln(2) split for range reduction in exp / log.
// LN2_HI has 20 trailing zero mantissa bits so k * LN2_HI is exact for |k| <= 2048.
static const double LN2_HI  = 0.69314718036912381649017333984375;
static const double LN2_LO  = 1.90821492927058770002e-10;
static const double INV_LN2 = 1.44269504088896340735992468100189;

// -----------------------------------------------------------------------------
// Floor / Ceil / Fmod
// -----------------------------------------------------------------------------

double base_floor(double x) {
    if (x >= 4503599627370496.0 || x <= -4503599627370496.0 || x != x) return x;
    int64_t i = (int64_t)x;
    double d = (double)i;
    return (d > x) ? (d - 1.0) : d;
}

float base_floorf(float x) {
    if (x >= 8388608.0f || x <= -8388608.0f || x != x) return x;
    int32_t i = (int32_t)x;
    float f = (float)i;
    return (f > x) ? (f - 1.0f) : f;
}

double base_ceil(double x) {
    if (x >= 4503599627370496.0 || x <= -4503599627370496.0 || x != x) return x;
    int64_t i = (int64_t)x;
    double d = (double)i;
    return (d < x) ? (d + 1.0) : d;
}

float base_ceilf(float x) {
    if (x >= 8388608.0f || x <= -8388608.0f || x != x) return x;
    int32_t i = (int32_t)x;
    float f = (float)i;
    return (f < x) ? (f + 1.0f) : f;
}

double base_fmod(double x, double y) {
    if (y == 0.0 || x != x || y != y) return (double)NAN;
    double ay = y < 0.0 ? -y : y;
    double q = x / ay;
    double iq = (q >= 0.0) ? base_floor(q) : base_ceil(q);
    return x - iq * ay;
}

float base_fmodf(float x, float y) {
    if (y == 0.0f || x != x || y != y) return NAN;
    float ay = y < 0.0f ? -y : y;
    float q = x / ay;
    float iq = (q >= 0.0f) ? base_floorf(q) : base_ceilf(q);
    return x - iq * ay;
}

// -----------------------------------------------------------------------------
// Double-precision trig: fast_sin, fast_cos, fast_tan
// Valid for |x| <= ~1.6e6 with < 5e-16 relative accuracy.
// -----------------------------------------------------------------------------

static inline double sin_poly_d(double r) {
    double r2 = r * r;
    double r4 = r2 * r2;
    double r8 = r4 * r4;
    // Odd Taylor polynomial of degree 15 on [-pi/4, pi/4]: |err| < 5e-17
    double p0 = -1.66666666666666657415e-01 + r2 *  8.33333333333333321769e-03;
    double p1 = -1.98412698412698412525e-04 + r2 *  2.75573192239858925126e-06;
    double p2 = -2.50521083854417202248e-08 + r2 *  1.60590438368216133407e-10;
    double p3 = -7.64716373181981641472e-13;
    double poly = (p0 + r4 * p1) + r8 * (p2 + r4 * p3);
    return r + r * r2 * poly;
}

static inline double cos_poly_d(double r) {
    double r2 = r * r;
    double r4 = r2 * r2;
    double r8 = r4 * r4;
    // Even Taylor polynomial of degree 16 on [-pi/4, pi/4]: |err| < 2e-18
    double p0 =  4.16666666666666643537e-02 + r2 * -1.38888888888888894189e-03;
    double p1 =  2.48015873015873015657e-05 + r2 * -2.75573192239858882757e-07;
    double p2 =  2.08767569878681001874e-09 + r2 * -1.14707455977297245224e-11;
    double p3 =  4.77947733238738525433e-14;
    double poly = (p0 + r4 * p1) + r8 * (p2 + r4 * p3);
    return (1.0 - 0.5 * r2) + r4 * poly;
}

double fast_sin(double x) {
    double kf = x * INV_PI2_D;
    int64_t k = (int64_t)(kf >= 0.0 ? (kf + 0.5) : (kf - 0.5));
    double kd = (double)k;
    double r = ((x - kd * PI2_HI) - kd * PI2_MID) - kd * PI2_LO;
    int64_t q = k & 3;
    double res = (q & 1) ? cos_poly_d(r) : sin_poly_d(r);
    return (q & 2) ? -res : res;
}

double fast_cos(double x) {
    double kf = x * INV_PI2_D;
    int64_t k = (int64_t)(kf >= 0.0 ? (kf + 0.5) : (kf - 0.5));
    double kd = (double)k;
    double r = ((x - kd * PI2_HI) - kd * PI2_MID) - kd * PI2_LO;
    int64_t q = (k + 1) & 3;
    double res = (q & 1) ? cos_poly_d(r) : sin_poly_d(r);
    return (q & 2) ? -res : res;
}

double fast_tan(double x) {
    double kf = x * INV_PI2_D;
    int64_t k = (int64_t)(kf >= 0.0 ? (kf + 0.5) : (kf - 0.5));
    double kd = (double)k;
    double r = ((x - kd * PI2_HI) - kd * PI2_MID) - kd * PI2_LO;
    double s = sin_poly_d(r);
    double c = cos_poly_d(r);
    return (k & 1) ? (-c / s) : (s / c);
}

// -----------------------------------------------------------------------------
// Single-precision trig: fast_sinf, fast_cosf, fast_tanf
// Valid for |x| <= ~1e6 with < 3e-7 relative accuracy.
// Uses branchless magic-number rounding and branchless quadrant selection.
// -----------------------------------------------------------------------------

static inline float sin_poly_f(float r) {
    float r2 = r * r;
    float r4 = r2 * r2;
    float p0 = -1.66666667e-01f + r2 *  8.33333333e-03f;
    float p1 = -1.98412698e-04f + r2 *  2.75573192e-06f;
    return r + r * r2 * (p0 + r4 * p1);
}

static inline float cos_poly_f(float r) {
    float r2 = r * r;
    float r4 = r2 * r2;
    float p0 =  4.16666667e-02f + r2 * -1.38888889e-03f;
    float p1 =  2.48015873e-05f;
    return (1.0f - 0.5f * r2) + r4 * (p0 + r4 * p1);
}

float fast_sinf(float x) {
    double xd = (double)x;
    double kf = xd * INV_PI2_D;
    int64_t k = (int64_t)(kf >= 0.0 ? (kf + 0.5) : (kf - 0.5));
    double kd = (double)k;
    float r = (float)((xd - kd * PI2_HI) - kd * PI2_MID);
    int64_t q = k & 3;
    float res = (q & 1) ? cos_poly_f(r) : sin_poly_f(r);
    union { float f; uint32_t u; } out;
    out.f = res;
    out.u ^= (uint32_t)((q & 2) << 30);
    return out.f;
}

float fast_cosf(float x) {
    double xd = (double)x;
    double kf = xd * INV_PI2_D;
    int64_t k = (int64_t)(kf >= 0.0 ? (kf + 0.5) : (kf - 0.5));
    double kd = (double)k;
    float r = (float)((xd - kd * PI2_HI) - kd * PI2_MID);
    int64_t q = (k + 1) & 3;
    float res = (q & 1) ? cos_poly_f(r) : sin_poly_f(r);
    union { float f; uint32_t u; } out;
    out.f = res;
    out.u ^= (uint32_t)((q & 2) << 30);
    return out.f;
}

float fast_tanf(float x) {
    double xd = (double)x;
    double kf = xd * INV_PI2_D;
    int64_t k = (int64_t)(kf >= 0.0 ? (kf + 0.5) : (kf - 0.5));
    double kd = (double)k;
    float r = (float)((xd - kd * PI2_HI) - kd * PI2_MID);
    float s = sin_poly_f(r);
    float c = cos_poly_f(r);
    return (k & 1) ? (-c / s) : (s / c);
}

// -----------------------------------------------------------------------------
// Exponential: fast_exp, fast_expf
// -----------------------------------------------------------------------------

static inline double exp_poly_d(double r) {
    double r2 = r * r;
    double r4 = r2 * r2;
    double r8 = r4 * r4;
    // Degree-12 Taylor polynomial on [-ln(2)/2, ln(2)/2]: |err| < 2e-16
    double p0 = 1.0 + r;
    double p1 = 5.00000000000000000000e-01 + r * 1.66666666666666657415e-01;
    double p2 = 4.16666666666666643537e-02 + r * 8.33333333333333321769e-03;
    double p3 = 1.38888888888888894189e-03 + r * 1.98412698412698412525e-04;
    double p4 = 2.48015873015873015657e-05 + r * 2.75573192239858925126e-06;
    double p5 = 2.75573192239858882757e-07 + r * 2.50521083854417202248e-08;
    double p6 = 2.08767569878681001874e-09;
    return (p0 + r2 * p1) + r4 * (p2 + r2 * p3) + r8 * ((p4 + r2 * p5) + r4 * p6);
}

static inline double scale_pow2_d(double v, int64_t k) {
    if (k >= 1024) return (double)INFINITY;
    if (k <= -1023) {
        if (k < -1075) return 0.0;
        int64_t k1 = k / 2;
        int64_t k2 = k - k1;
        union { double d; uint64_t u; } s1;
        union { double d; uint64_t u; } s2;
        s1.u = (uint64_t)(k1 + 1023) << 52;
        s2.u = (uint64_t)(k2 + 1023) << 52;
        return (v * s1.d) * s2.d;
    }
    union { double d; uint64_t u; } s;
    s.u = (uint64_t)(k + 1023) << 52;
    return v * s.d;
}

double fast_exp(double x) {
    if (x > 709.782712893384) return (double)INFINITY;
    if (x < -745.1332191019412) return 0.0;
    double kf = x * INV_LN2;
    int64_t k = (int64_t)(kf >= 0.0 ? (kf + 0.5) : (kf - 0.5));
    double kd = (double)k;
    double r = (x - kd * LN2_HI) - kd * LN2_LO;
    return scale_pow2_d(exp_poly_d(r), k);
}

static inline float exp_poly_f(float r) {
    float r2 = r * r;
    float r4 = r2 * r2;
    float p0 = 1.0f + r;
    float p1 = 0.5f + r * 1.66666667e-01f;
    float p2 = 4.16666667e-02f + r * 8.33333333e-03f;
    float p3 = 1.38888889e-03f;
    return (p0 + r2 * p1) + r4 * (p2 + r2 * p3);
}

static inline float scale_pow2_f(float v, int32_t k) {
    if (k >= 128) return INFINITY;
    if (k <= -127) {
        if (k < -150) return 0.0f;
        int32_t k1 = k / 2;
        int32_t k2 = k - k1;
        union { float f; uint32_t u; } s1;
        union { float f; uint32_t u; } s2;
        s1.u = (uint32_t)(k1 + 127) << 23;
        s2.u = (uint32_t)(k2 + 127) << 23;
        return (v * s1.f) * s2.f;
    }
    union { float f; uint32_t u; } s;
    s.u = (uint32_t)(k + 127) << 23;
    return v * s.f;
}

float fast_expf(float x) {
    if (x > 88.722839f) return INFINITY;
    if (x < -103.97208f) return 0.0f;
    float kf = x * 1.4426950408889634f;
    int32_t k = (int32_t)(kf >= 0.0f ? (kf + 0.5f) : (kf - 0.5f));
    float kd = (float)k;
    float r = (x - kd * 0.693145751953125f) - kd * 1.428606765330187e-06f;
    return scale_pow2_f(exp_poly_f(r), k);
}

// -----------------------------------------------------------------------------
// Natural logarithm: fast_log, fast_logf
// -----------------------------------------------------------------------------

static inline double log_mantissa_d(double m) {
    // m in [sqrt(2)/2, sqrt(2)], so |t| <= 0.171573, s = t^2 <= 0.029437
    double t = (m - 1.0) / (m + 1.0);
    double s = t * t;
    double s2 = s * s;
    double s4 = s2 * s2;
    double s8 = s4 * s4;
    // ln(m) = 2*t * sum_{k=0..9} s^k / (2k+1), |err| < 5e-17
    double q0 = 1.0                        + s * 3.33333333333333314830e-01;
    double q1 = 2.00000000000000011102e-01 + s * 1.42857142857142849213e-01;
    double q2 = 1.11111111111111104943e-01 + s * 9.09090909090909116141e-02;
    double q3 = 7.69230769230769273470e-02 + s * 6.66666666666666657415e-02;
    double q4 = 5.88235294117647050666e-02 + s * 5.26315789473684181270e-02;
    double poly = (q0 + s2 * q1) + s4 * (q2 + s2 * q3) + s8 * q4;
    return 2.0 * t * poly;
}

double fast_log(double x) {
    if (x <= 0.0) return (x == 0.0) ? -(double)INFINITY : (double)NAN;
    union { double d; uint64_t u; } b;
    b.d = x;
    int64_t e = (int64_t)((b.u >> 52) & 0x7FFULL) - 1023;
    uint64_t frac = b.u & 0x000FFFFFFFFFFFFFULL;
    if (frac > 0x0006A09E667F3BCDULL) {
        e = e + 1;
        b.u = frac | 0x3FE0000000000000ULL;
    } else {
        b.u = frac | 0x3FF0000000000000ULL;
    }
    double lm = log_mantissa_d(b.d);
    double ed = (double)e;
    return ed * LN2_HI + (ed * LN2_LO + lm);
}

static inline float log_mantissa_f(float m) {
    float t = (m - 1.0f) / (m + 1.0f);
    float s = t * t;
    float s2 = s * s;
    float q0 = 1.0f + s * 3.33333333e-01f;
    float q1 = 2.00000000e-01f + s * 1.42857143e-01f;
    float q2 = 1.11111111e-01f;
    return 2.0f * t * ((q0 + s2 * q1) + s2 * s2 * q2);
}

float fast_logf(float x) {
    if (x <= 0.0f) return (x == 0.0f) ? -INFINITY : NAN;
    union { float f; uint32_t u; } b;
    b.f = x;
    int32_t e = (int32_t)((b.u >> 23) & 0xFFU) - 127;
    uint32_t frac = b.u & 0x007FFFFFU;
    if (frac > 0x003504F3U) {
        e = e + 1;
        b.u = frac | 0x3F000000U;
    } else {
        b.u = frac | 0x3F800000U;
    }
    return (float)e * 0.6931471805599453f + log_mantissa_f(b.f);
}

// -----------------------------------------------------------------------------
// Power: fast_pow, fast_powf
// Uses hi+lo decomposition of y * log2(x) so that even large products retain
// >= 1e-15 relative accuracy in double precision.
// -----------------------------------------------------------------------------

double fast_pow(double x, double y) {
    if (y == 0.0 || x == 1.0) return 1.0;
    if (y == 1.0) return x;
    if (x <= 0.0) {
        if (x == 0.0) return (y > 0.0) ? 0.0 : (double)INFINITY;
        double iy = base_floor(y);
        if (iy != y) return (double)NAN;
        double res = fast_pow(-x, y);
        return ((int64_t)iy & 1) ? -res : res;
    }

    // Decompose x = 2^e * m with m in [sqrt(2)/2, sqrt(2)]
    union { double d; uint64_t u; } b;
    b.d = x;
    int64_t e = (int64_t)((b.u >> 52) & 0x7FFULL) - 1023;
    uint64_t frac = b.u & 0x000FFFFFFFFFFFFFULL;
    if (frac > 0x0006A09E667F3BCDULL) {
        e = e + 1;
        b.u = frac | 0x3FE0000000000000ULL;
    } else {
        b.u = frac | 0x3FF0000000000000ULL;
    }

    // Compute log2(x) = e + log2(m) split into exact-ish hi (24-bit) + lo
    double lm = log_mantissa_d(b.d);
    double log2_m = lm * INV_LN2;
    double l_hi = (double)e + (double)(float)log2_m;
    double l_lo = log2_m - (double)(float)log2_m;

    // Split y into 24-bit y_hi + y_lo so y_hi * l_hi is exact in double
    double y_hi = (double)(float)y;
    double y_lo = y - y_hi;

    // p = y * log2(x) = p_hi + p_lo
    double p_hi = y_hi * l_hi;
    double p_lo = y_hi * l_lo + y_lo * (l_hi + l_lo);

    double p_tot = p_hi + p_lo;
    if (p_tot > 1024.0) return (double)INFINITY;
    if (p_tot < -1075.0) return 0.0;

    int64_t k = (int64_t)(p_tot >= 0.0 ? (p_tot + 0.5) : (p_tot - 0.5));
    double f = (p_hi - (double)k) + p_lo; // |f| <= 0.5
    double r = f * LN2_HI + f * LN2_LO;   // |r| <= ln(2)/2
    return scale_pow2_d(exp_poly_d(r), k);
}

float fast_powf(float x, float y) {
    if (y == 0.0f || x == 1.0f) return 1.0f;
    if (y == 1.0f) return x;
    if (x <= 0.0f) {
        if (x == 0.0f) return (y > 0.0f) ? 0.0f : INFINITY;
        float iy = base_floorf(y);
        if (iy != y) return NAN;
        float res = fast_powf(-x, y);
        return ((int32_t)iy & 1) ? -res : res;
    }

    // Inline fast log2(x) in double (short 5-term poly is exact to >28 bits)
    union { float f; uint32_t u; } b;
    b.f = x;
    int32_t e = (int32_t)((b.u >> 23) & 0xFFU) - 127;
    uint32_t frac = b.u & 0x007FFFFFU;
    if (frac > 0x003504F3U) {
        e = e + 1;
        b.u = frac | 0x3F000000U;
    } else {
        b.u = frac | 0x3F800000U;
    }
    double m = (double)b.f;
    double t = (m - 1.0) / (m + 1.0);
    double s = t * t;
    double s2 = s * s;
    double lm = 2.0 * t * ((1.0 + s * (1.0 / 3.0)) + s2 * (0.2 + s * (1.0 / 7.0)) + s2 * s2 * (1.0 / 9.0));
    double p = (double)y * ((double)e + lm * INV_LN2);
    if (p > 128.0) return INFINITY;
    if (p < -150.0) return 0.0f;
    int32_t k = (int32_t)(p >= 0.0 ? (p + 0.5) : (p - 0.5));
    float r = (float)((p - (double)k) * 0.6931471805599453);
    return scale_pow2_f(exp_poly_f(r), k);
}
