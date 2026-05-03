#pragma once

// Basic integer types for nostdlib builds
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

// On Linux x86_64, long is 64 bits, so use unsigned long for uint64_t
// On other platforms (especially Windows), long is 32 bits, so use unsigned long long
#if defined(__linux__) && defined(__x86_64__)
typedef unsigned long uint64_t;
#else
typedef unsigned long long uint64_t;
#endif

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;

#if defined(__linux__) && defined(__x86_64__)
typedef long int64_t;
#else
typedef signed long long int64_t;
#endif

// Pointer-sized integer type and size types (only for nostdlib builds)

#if defined(_WIN32) && defined(_WIN64)
    // For 64 bit Windows the long is 4 bytes, but pointer is 8 bytes
    typedef uint64_t uintptr_t;
    typedef uint64_t ptrdiff_t;
#else
    // For 32 bit platforms and wasm64 the long and a pointer is 4 bytes, for
    // 64 bit macOS/Linux the long and pointer is 8 bytes
    typedef unsigned long uintptr_t;
    typedef long ptrdiff_t;
#endif

#if defined(_WIN32) && defined(_WIN64)
    // 64 bit Windows has 8 byte size_t (but 4 byte long)
    typedef uint64_t size_t;
    typedef int64_t ssize_t;
#else
    // All other platforms have long and size_t the same number of bytes (4 or
    // 8)
    typedef unsigned long size_t;
    typedef signed long ssize_t;
#endif


// NULL, boolean types, and limits

#define NULL ((void*)0)

// Boolean type and constants
#define bool _Bool
#define true 1
#define false 0
#define SIZE_MAX ((size_t)-1)

#define INT8_C(value) value
#define UINT8_C(value) value##u
#define INT16_C(value) value
#define UINT16_C(value) value##u
#define INT32_C(value) value
#define UINT32_C(value) value##u
#define INT64_C(value) value##ll
#define UINT64_C(value) value##ull
#define INTMAX_C(value) INT64_C(value)
#define UINTMAX_C(value) UINT64_C(value)
#define UINT16_MAX ((uint16_t)0xFFFFu)
#define INT32_MAX ((int32_t)0x7FFFFFFF)
#define UINT32_MAX ((uint32_t)0xFFFFFFFFu)
#define INT64_MAX ((int64_t)0x7FFFFFFFFFFFFFFFll)
#define UINT64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFFull)
#define FLT_MAX 3.402823466e+38F
