#pragma once

#include <base/types.h>

// We build with -nostdinc / /X, so <math.h> is not available. Define the
// usual C99 floating-point macros ourselves. The expression
// `(float)(1e308 * 1e308)` overflows to +inf at compile time on every
// conforming compiler (Clang, GCC, MSVC), so no compiler-specific spelling
// is needed.

#ifdef __cplusplus
extern "C" {
#endif

#ifndef INFINITY
#define INFINITY ((float)(1e308 * 1e308))
#endif

#ifndef NAN
#define NAN ((float)(INFINITY * 0.0f))
#endif

#ifndef HUGE_VAL
#define HUGE_VAL ((double)INFINITY)
#endif

static inline double base_fabs(double x) {
    return x < 0.0 ? -x : x;
}

static inline float base_fabsf(float x) {
    return x < 0.0f ? -x : x;
}

// Simple round implementation. Note: Overflows for values outside [INT64_MIN, INT64_MAX].
static inline double base_round(double x) {
    return (x >= 0.0) ? (double)(int64_t)(x + 0.5) : (double)(int64_t)(x - 0.5);
}

static inline float base_roundf(float x) {
    return (x >= 0.0f) ? (float)(int64_t)(x + 0.5f) : (float)(int64_t)(x - 0.5f);
}

// Fast rounding / remainder helpers
double base_floor(double x);
float  base_floorf(float x);
double base_ceil(double x);
float  base_ceilf(float x);
double base_fmod(double x, double y);
float  base_fmodf(float x, float y);

// Fast double-precision math functions (>= 1e-14 relative accuracy)
double fast_sin(double x);
double fast_cos(double x);
double fast_tan(double x);
double fast_exp(double x);
double fast_log(double x);
double fast_pow(double x, double y);

// Fast single-precision math functions (< 1e-6 relative accuracy)
float fast_sinf(float x);
float fast_cosf(float x);
float fast_tanf(float x);
float fast_expf(float x);
float fast_logf(float x);
float fast_powf(float x, float y);

// Fast square-root functions (declared in platform.h, repeated here for
// discoverability alongside the other math primitives).
double fast_sqrt(double x);
float  fast_sqrtf(float x);

#ifdef __cplusplus
}
#endif
