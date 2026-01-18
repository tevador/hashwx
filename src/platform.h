/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#include <stdint.h>
#include <string.h>
#include <assert.h>

#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _MSC_VER
#pragma warning(error: 4013) /* calls to undefined functions */
#pragma warning(error: 4090) /* different const qualifiers */
#pragma warning(error: 4133) /* incompatible pointer types */
#pragma warning(disable: 4146) /* unary minus applied to unsigned type */
#endif

static_assert(~0 == -1, "Only two's complement signed integers are supported");

/* inline */
#if defined(_MSC_VER)
#define INLINE __inline
#elif defined(__GNUC__) || defined(__clang__)
#define INLINE __inline__
#else
#define INLINE inline
#endif

/* force inline */
#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE __attribute__((always_inline)) __inline__
#else
#define FORCE_INLINE INLINE
#endif

/* never inline */
#if defined(_MSC_VER)
#define NEVER_INLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define NEVER_INLINE __attribute__ ((noinline))
#else
#define NEVER_INLINE
#endif

/* unreachable code */
#ifndef UNREACHABLE
#ifdef __GNUC__
#define UNREACHABLE __builtin_unreachable()
#elif _MSC_VER
#define UNREACHABLE __assume(0)
#else
#define UNREACHABLE
#endif
#endif

/* detect native little-endian platforms */
#if (defined(__BYTE_ORDER__) &&                                                \
     (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)) ||                           \
    defined(__LITTLE_ENDIAN__) || defined(__ARMEL__) || defined(__MIPSEL__) || \
    defined(__AARCH64EL__) || defined(__amd64__) || defined(__i386__) ||       \
    defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64) ||                \
    defined(_M_ARM)
#define PLATFORM_LE
#endif
/* platforms not listed above will use endian-agnostic code */

/* loads/stores in little endian format */
static FORCE_INLINE uint32_t platform_load32(const void* src) {
#if defined(PLATFORM_LE)
    uint32_t w;
    memcpy(&w, src, sizeof w);
    return w;
#else
    const uint8_t* p = (const uint8_t*)src;
    uint32_t w = *p++;
    w |= (uint32_t)(*p++) << 8;
    w |= (uint32_t)(*p++) << 16;
    w |= (uint32_t)(*p++) << 24;
    return w;
#endif
}

static FORCE_INLINE uint64_t platform_load64(const void* src) {
#if defined(PLATFORM_LE)
    uint64_t w;
    memcpy(&w, src, sizeof w);
    return w;
#else
    const uint8_t* p = (const uint8_t*)src;
    uint64_t w = *p++;
    w |= (uint64_t)(*p++) << 8;
    w |= (uint64_t)(*p++) << 16;
    w |= (uint64_t)(*p++) << 24;
    w |= (uint64_t)(*p++) << 32;
    w |= (uint64_t)(*p++) << 40;
    w |= (uint64_t)(*p++) << 48;
    w |= (uint64_t)(*p++) << 56;
    return w;
#endif
}

double platform_wall_clock(void);

#endif /* PLATFORM_H */
