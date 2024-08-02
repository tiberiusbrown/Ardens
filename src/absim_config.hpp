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
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#define ARDENS_ARCH_X86_32
#endif

#ifdef ARDENS_ARCH_X86_64
#define ARDENS_SSE2
#endif
