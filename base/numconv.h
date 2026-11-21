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

// Convert double to string with specified precision
// Returns length of string written (not including null terminator)
// precision: number of decimal places (-1 for default of 6)
size_t double_to_str(double val, char* buf, int precision);
size_t double_to_str_e(double val, char* buf, int precision);

// Convert unsigned 64-bit integer to hexadecimal string
// Returns length of string written (not including null terminator)
// uppercase: 0 for lowercase (a-f), non-zero for uppercase (A-F)
size_t uint64_to_hex_str(uint64_t val, char* buf, int uppercase);

// vsnprintf/snprintf for base/ (used by the nostdlib C library layer).
// Follows C's vsnprintf contract: writes at most `size` bytes including the
// terminating NUL (always NUL-terminates when size > 0; `str` may be NULL when
// size == 0) and returns the length the full output would have (not counting
// the NUL), so callers can measure with (NULL, 0) and detect truncation.
// Supports:
//   flags: - 0 + space #   width and precision: N or *
//   length modifiers: hh h l ll j z t
//   conversions: d i u o x X p c s f e g %
// Limitations: %f/%e/%g use a simple algorithm (fractional digits are
// truncated, not correctly rounded; %g never switches to exponent form;
// %e prints at most 17 fractional digits); '#' has no effect on f/e/g.
int base_vsnprintf(char *str, size_t size, const char *format, va_list args);
int base_snprintf(char *str, size_t size, const char *format, ...);
#ifdef __cplusplus
}
#endif
