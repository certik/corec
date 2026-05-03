#pragma once

#include <base/base_types.h>

#ifndef NAN
#ifdef _MSC_VER
#define NAN ((float)(INFINITY * 0.0f))
#else
#define NAN (__builtin_nanf(""))
#endif
#endif

#ifndef INFINITY
#ifdef _MSC_VER
// Use the standard C99 macro from math.h or define it directly
#include <math.h>
#ifndef INFINITY
#define INFINITY ((float)(1e308 * 1e308))
#endif
#else
#define INFINITY (__builtin_inff())
#endif
#endif

#ifndef HUGE_VAL
#ifdef _MSC_VER
#include <math.h>
#ifndef HUGE_VAL
#define HUGE_VAL ((double)INFINITY)
#endif
#else
#define HUGE_VAL (__builtin_huge_val())
#endif
#endif

static inline double base_fabs(double x) {
    return x < 0 ? -x : x;
}

static inline float base_fabsf(float x) {
    return x < 0 ? -x : x;
}

// Simple round implementation. Note: Overflows for values outside [INT64_MIN, INT64_MAX].
static inline double base_round(double x) {
    return (x >= 0.0) ? (double)(int64_t)(x + 0.5) : (double)(int64_t)(x - 0.5);
}

// Fast single-precision trigonometric functions
float fast_sin(float x);
float fast_cos(float x);
float fast_tan(float x);
