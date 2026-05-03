#pragma once

// Variadic arguments support
// This provides cross-platform variadic argument macros using compiler builtins

#if defined(_MSC_VER)
// MSVC varargs implementation
// x86: 4-byte alignment, x64/ARM64: 8-byte alignment
typedef char* va_list;

#if defined(_M_X64) || defined(_M_ARM64)
#define _VA_ALIGN 8
#else
#define _VA_ALIGN 4
#endif

#define _VA_ROUNDED_SIZE(t) ((sizeof(t) + _VA_ALIGN - 1) & ~(_VA_ALIGN - 1))

#define va_start(ap, v) ((void)((ap) = (va_list)((char*)(&(v)) + _VA_ROUNDED_SIZE(v))))
#define va_arg(ap, t) (*(t*)((ap += _VA_ROUNDED_SIZE(t)) - _VA_ROUNDED_SIZE(t)))
#define va_end(ap) ((void)((ap) = (va_list)0))
#define va_copy(dest, src) ((dest) = (src))

#else
// Use compiler builtins for Clang/GCC
typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start((ap), (last))
#define va_arg(ap, type) __builtin_va_arg((ap), type)
#define va_end(ap) __builtin_va_end((ap))
#define va_copy(dest, src) __builtin_va_copy((dest), (src))

#endif
