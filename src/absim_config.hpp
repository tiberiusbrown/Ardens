#pragma once

#if defined(_MSC_VER)

#define ARDENS_FORCEINLINE __forceinline
#define ARDENS_NOINLINE __declspec(noinline)

#elif defined(__GNUC__) || defined(__clang__) 

#define ARDENS_FORCEINLINE [[gnu::always_inline]] inline
#define ARDENS_NOINLINE [[gnu::noinline]]

#else

#define ARDENS_FORCEINLINE inline
#define ARDENS_NOINLINE

#endif

#if defined(__x86_64__) || defined(_M_X64)
#define ARDENS_ARCH_X86_64
#define ARDENS_ARCH_X86
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#define ARDENS_ARCH_X86_32
#define ARDENS_ARCH_X86
#elif defined(__arm__) || defined(_M_ARM)
#define ARDENS_ARCH_ARM
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARDENS_ARCH_ARM64
#define ARDENS_ARCH_ARM
#endif

#ifdef ARDENS_ARCH_X86_64
#define ARDENS_SSE2
#endif

#if defined(ARDENS_ARCH_X86) || defined(ARDENS_ARCH_ARM)
#define ARDENS_LE
#elif defined(__GNUC__) || defined(__clang__)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define ARDENS_LE
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ARDENS_BE
#else
#error "unknown endianness"
#endif
#elif !defined(__APPLE__) && !defined(_WIN32)
#include <endian.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ARDENS_LE
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ARDENS_BE
#else
#error "unknown endianness"
#endif
#else
#error "unknown endianness"
#endif
