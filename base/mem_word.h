// Word-at-a-time memcpy / memmove / memset.
//
// Shared by base/mem.c (base_memcpy, base_memmove, base_memset) and the weak
// memcpy/memset fallbacks in platform/platform_linux.c. The includer names the
// functions to define, then includes this file:
//
//     #define BASE_MEM_WORD_MEMCPY  base_memcpy
//     #define BASE_MEM_WORD_MEMMOVE base_memmove   // optional
//     #define BASE_MEM_WORD_MEMSET  base_memset
//     #define BASE_MEM_WORD_ATTR    __attribute__((weak))  // optional
//     #include <base/mem_word.h>
//
// When BASE_MEM_WORD_MEMMOVE is defined, BASE_MEM_WORD_MEMCPY must be a corec
// name (such as base_memcpy), never the real memcpy: the memmove calls it for
// overlapping regions when dest is below src, which is correct for this
// implementation but not for a call the compiler knows as memcpy (it may
// assume that memcpy's arguments do not overlap). platform_linux.c defines the
// real memcpy and memset, and no memmove.
//
// The functions are defined directly under the requested names, not as shared
// helpers: tinyC gives `static` functions external linkage (two TUs with the
// same static helper would clash at link time), and it does not inline, so a
// wrapper would cost an extra call per copy.
//
// Algorithm: when n >= one word and dest and src have the same address modulo
// the word size, copy single bytes until dest is word aligned, then copy words
// (four per iteration, then one at a time), then the remaining tail bytes.
// When the alignments differ, copy bytes, eight per iteration. memset only
// needs dest to be aligned, and stores the byte broadcast into every byte of a
// word.
//
// The word is uintptr_t: 8 bytes on 64-bit targets, 4 bytes on wasm32.
//
// The loops are plain C (no intrinsics), so tinyC compiles them as is.

// Common part: included once per TU, even if the template below is
// instantiated twice (e.g. a single-TU build of base/mem.c and
// platform/platform_linux.c).
#ifndef BASE_MEM_WORD_COMMON
#define BASE_MEM_WORD_COMMON

#include <base/types.h>

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
// The words alias the caller's bytes, whatever their declared type, so they
// must be exempt from type-based (strict) aliasing analysis.
typedef uintptr_t __attribute__((may_alias)) base_mem_word_t;

// Clang and GCC recognise copy and fill loops (byte or word) and rewrite them
// into a call to memcpy/memmove/memset (clang's loop-idiom recognition, GCC's
// -ftree-loop-distribute-patterns). Inside the functions that implement
// memcpy/memset this is unbounded recursion (stack overflow on the first
// call), and base_memcpy etc. back the memcpy of corec-stdlib. -fno-builtin
// prevents it, but hosted builds (e.g. -O3 -flto) do not pass it: without the
// barrier, gcc -O2 turns base_memset into a memset call and clang -O2 turns
// memset into a call to itself. A volatile asm with a "memory" clobber in
// every loop iteration is a side effect the optimiser must keep, so the loop
// is no longer a pure copy/fill idiom. It emits no instructions. clang-cl
// defines __clang__ but not __GNUC__, and needs the barrier too (at /O1 and
// /O2 it turns base_memset into a memset call). tinyC does no such transform
// and has no inline asm, and MSVC (cl) has no strict aliasing, so they take
// the #else branch.
#define BASE_MEM_WORD_BARRIER() __asm__ volatile("" ::: "memory")
#else
typedef uintptr_t base_mem_word_t;
#define BASE_MEM_WORD_BARRIER()
#endif

#define BASE_MEM_WORD_SIZE (sizeof(base_mem_word_t))
#define BASE_MEM_WORD_MASK (BASE_MEM_WORD_SIZE - 1)

#endif // BASE_MEM_WORD_COMMON

#ifndef BASE_MEM_WORD_ATTR
#define BASE_MEM_WORD_ATTR
#endif

// Copy forward. Also used by BASE_MEM_WORD_MEMMOVE when dest < src: every
// group of words is loaded before it is stored, and a store to dest+i never
// reaches src+j for j > i, so overlapping regions with dest below src are
// copied correctly.
BASE_MEM_WORD_ATTR
void* BASE_MEM_WORD_MEMCPY(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    if (n >= BASE_MEM_WORD_SIZE &&
            (((uintptr_t)d ^ (uintptr_t)s) & BASE_MEM_WORD_MASK) == 0) {
        while (((uintptr_t)d & BASE_MEM_WORD_MASK) != 0) {
            *d++ = *s++;
            n--;
            BASE_MEM_WORD_BARRIER();
        }
        base_mem_word_t* dw = (base_mem_word_t*)d;
        const base_mem_word_t* sw = (const base_mem_word_t*)s;
        while (n >= 4 * BASE_MEM_WORD_SIZE) {
            base_mem_word_t w0 = sw[0];
            base_mem_word_t w1 = sw[1];
            base_mem_word_t w2 = sw[2];
            base_mem_word_t w3 = sw[3];
            dw[0] = w0;
            dw[1] = w1;
            dw[2] = w2;
            dw[3] = w3;
            dw += 4;
            sw += 4;
            n -= 4 * BASE_MEM_WORD_SIZE;
            BASE_MEM_WORD_BARRIER();
        }
        while (n >= BASE_MEM_WORD_SIZE) {
            *dw++ = *sw++;
            n -= BASE_MEM_WORD_SIZE;
            BASE_MEM_WORD_BARRIER();
        }
        d = (unsigned char*)dw;
        s = (const unsigned char*)sw;
    }
    // Different alignments: bytes, eight per iteration. Loading all eight
    // before storing them lets gcc/clang merge the group into one unaligned
    // 8-byte load and store; tinyC copies them one by one. Then the tail.
    while (n >= 8) {
        unsigned char b0 = s[0];
        unsigned char b1 = s[1];
        unsigned char b2 = s[2];
        unsigned char b3 = s[3];
        unsigned char b4 = s[4];
        unsigned char b5 = s[5];
        unsigned char b6 = s[6];
        unsigned char b7 = s[7];
        d[0] = b0;
        d[1] = b1;
        d[2] = b2;
        d[3] = b3;
        d[4] = b4;
        d[5] = b5;
        d[6] = b6;
        d[7] = b7;
        d += 8;
        s += 8;
        n -= 8;
        BASE_MEM_WORD_BARRIER();
    }
    while (n != 0) {
        *d++ = *s++;
        n--;
        BASE_MEM_WORD_BARRIER();
    }
    return dest;
}

#ifdef BASE_MEM_WORD_MEMMOVE
BASE_MEM_WORD_ATTR
void* BASE_MEM_WORD_MEMMOVE(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;

    if (d == s || n == 0) {
        return dest;
    }

    // dest below src, or no overlap: the forward copy is safe (see above).
    if (d < s || d >= s + n) {
        return BASE_MEM_WORD_MEMCPY(dest, src, n);
    }

    // dest overlaps the end of src: copy backward, from the end. Symmetric to
    // the forward copy: every group of words is loaded before it is stored,
    // and a store to dest+i never reaches src+j for j < i.
    d += n;
    s += n;
    if (n >= BASE_MEM_WORD_SIZE &&
            (((uintptr_t)d ^ (uintptr_t)s) & BASE_MEM_WORD_MASK) == 0) {
        while (((uintptr_t)d & BASE_MEM_WORD_MASK) != 0) {
            *--d = *--s;
            n--;
            BASE_MEM_WORD_BARRIER();
        }
        base_mem_word_t* dw = (base_mem_word_t*)d;
        const base_mem_word_t* sw = (const base_mem_word_t*)s;
        while (n >= 4 * BASE_MEM_WORD_SIZE) {
            dw -= 4;
            sw -= 4;
            base_mem_word_t w3 = sw[3];
            base_mem_word_t w2 = sw[2];
            base_mem_word_t w1 = sw[1];
            base_mem_word_t w0 = sw[0];
            dw[3] = w3;
            dw[2] = w2;
            dw[1] = w1;
            dw[0] = w0;
            n -= 4 * BASE_MEM_WORD_SIZE;
            BASE_MEM_WORD_BARRIER();
        }
        while (n >= BASE_MEM_WORD_SIZE) {
            *--dw = *--sw;
            n -= BASE_MEM_WORD_SIZE;
            BASE_MEM_WORD_BARRIER();
        }
        d = (unsigned char*)dw;
        s = (const unsigned char*)sw;
    }
    // Different alignments: bytes, eight per iteration (see the forward copy).
    while (n >= 8) {
        d -= 8;
        s -= 8;
        unsigned char b7 = s[7];
        unsigned char b6 = s[6];
        unsigned char b5 = s[5];
        unsigned char b4 = s[4];
        unsigned char b3 = s[3];
        unsigned char b2 = s[2];
        unsigned char b1 = s[1];
        unsigned char b0 = s[0];
        d[7] = b7;
        d[6] = b6;
        d[5] = b5;
        d[4] = b4;
        d[3] = b3;
        d[2] = b2;
        d[1] = b1;
        d[0] = b0;
        n -= 8;
        BASE_MEM_WORD_BARRIER();
    }
    while (n != 0) {
        *--d = *--s;
        n--;
        BASE_MEM_WORD_BARRIER();
    }
    return dest;
}
#endif

BASE_MEM_WORD_ATTR
void* BASE_MEM_WORD_MEMSET(void* dest, int c, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    unsigned char b = (unsigned char)c;
    if (n >= BASE_MEM_WORD_SIZE) {
        while (((uintptr_t)d & BASE_MEM_WORD_MASK) != 0) {
            *d++ = b;
            n--;
            BASE_MEM_WORD_BARRIER();
        }
        // 0x0101...01 times the byte puts the byte into every byte of a word.
        base_mem_word_t w = (base_mem_word_t)(c & 0xFF) * ((base_mem_word_t)-1 / 0xFF);
        base_mem_word_t* dw = (base_mem_word_t*)d;
        while (n >= 4 * BASE_MEM_WORD_SIZE) {
            dw[0] = w;
            dw[1] = w;
            dw[2] = w;
            dw[3] = w;
            dw += 4;
            n -= 4 * BASE_MEM_WORD_SIZE;
            BASE_MEM_WORD_BARRIER();
        }
        while (n >= BASE_MEM_WORD_SIZE) {
            *dw++ = w;
            n -= BASE_MEM_WORD_SIZE;
            BASE_MEM_WORD_BARRIER();
        }
        d = (unsigned char*)dw;
    }
    while (n != 0) {
        *d++ = b;
        n--;
        BASE_MEM_WORD_BARRIER();
    }
    return dest;
}

#undef BASE_MEM_WORD_MEMCPY
#undef BASE_MEM_WORD_MEMMOVE
#undef BASE_MEM_WORD_MEMSET
#undef BASE_MEM_WORD_ATTR
