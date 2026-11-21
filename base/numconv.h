#pragma once

#include <base/types.h>
#include <base/stdarg.h>

// Number to string conversion functions for base/
// Self-contained implementations with no external dependencies

// Convert unsigned 64-bit integer to string
// Returns length of string written (not including null terminator)

#ifdef __cplusplus
extern "C" {
#endif

size_t uint64_to_str(uint64_t val, char* buf);

// Convert signed 64-bit integer to string
// Returns length of string written (not including null terminator)
size_t int64_to_str(int64_t val, char* buf);

// Convert int to string (calls int64_to_str)
// Returns length of string written (not including null terminator)
size_t int_to_str(int val, char* buf);

// Convert double to string with specified precision ("%.<precision>f").
// The integer part is exact; the fractional digits are truncated, not rounded.
// Returns length of string written (not including null terminator)
// precision: number of decimal places (-1 for default of 6)
size_t double_to_str(double val, char* buf, int precision);
// "%.<precision>e" (precision clamped to 17).
size_t double_to_str_e(double val, char* buf, int precision);

// vsnprintf/snprintf for base/ (used by the nostdlib C library layer).
// Follows C's vsnprintf contract: writes at most `size` bytes including the
// terminating NUL (always NUL-terminates when size > 0; `str` may be NULL when
// size == 0) and returns the length the full output would have (not counting
// the NUL), so callers can measure with (NULL, 0) and detect truncation.
// Supports:
//   flags: - 0 + space #   width and precision: N or *
//   length modifiers: hh h l ll j z t
//   conversions: d i u o x X p c s f F e E g G %
// Unsupported conversions that take an argument (%n, %a, %A, and the L
// modifier on f/e/g) consume it and print nothing; %n stores nothing.
// Float limitations:
//   - %f truncates the fractional digits instead of rounding them, and its
//     precision is capped at 40; the integer part is always exact.
//   - %e/%g compute the digits in double arithmetic, so they are correctly
//     rounded only to about 15 significant digits; precision is capped at 17
//     for %e and at 18 significant digits for %g; exact decimal ties round
//     up (C rounds them to even, e.g. "%.3e" of 12345 is 1.235e+04, not
//     1.234e+04).
// Returns -1 if the length would exceed INT_MAX.
int base_vsnprintf(char *str, size_t size, const char *format, va_list args);
int base_snprintf(char *str, size_t size, const char *format, ...);
#ifdef __cplusplus
}
#endif
