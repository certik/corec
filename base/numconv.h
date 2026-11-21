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

// Convert double to string like "%.<precision>f" (precision -1 means 6; it
// is capped at 40). Writes at most 350 bytes (DBL_MAX has 309 integer digits),
// so a 360-byte buffer is always enough. Returns the length written (no NUL).
size_t double_to_str(double val, char* buf, int precision);
// Like "%.<precision>e" (precision -1 means 6; it is capped at 40).
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
// Float limitations (exact decimal conversion is not implemented):
//   - %f: the integer part is exact. The fraction is rounded once in double
//     arithmetic (half up on the binary value; C rounds exact binary ties
//     such as 0.125 -> "0.12" to even) to at most 17 digits; further digits
//     print as '0'. Precision is capped at 40. Fraction digits past about 15
//     significant digits of the value are not exact.
//   - %e/%g: the digits come from repeated scaling by 10 and are reliable to
//     about 14 significant digits (at 15, about 1 value in 10 differs in the
//     last digit); digits past the 18th print as '0'. A value very close to
//     a rounding tie (within about 1e-15 relative) can round either way at
//     any precision (e.g. "%.2e" of 8435000.0 gives 8.43e+06, "%g" of
//     999999.5 gives 999999). %e precision is capped at 40, %g at 340.
// Returns -1 if the length would exceed INT_MAX.
int base_vsnprintf(char *str, size_t size, const char *format, va_list args);
int base_snprintf(char *str, size_t size, const char *format, ...);
#ifdef __cplusplus
}
#endif
