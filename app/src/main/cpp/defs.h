#pragma once

// Minimal Hex-Rays compatibility layer for the recovered ARM64 C++ output.
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <pthread.h>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdio>
#include <unistd.h>
#include <stdarg.h>
#include <type_traits>
#include <sys/mman.h>
#include <dlfcn.h>

#define __int64 long long
#define __int32 int
#define __int16 short
#define __int8 char
#define __fastcall
#define __noreturn __attribute__((noreturn))
#define _BYTE unsigned char
#define _WORD unsigned short
#define _DWORD unsigned int
#define _QWORD unsigned long long
#define _UNKNOWN unsigned long long
#define _BOOL8 long long
#define _BOOL4 int
#define gcc_va_list va_list
#define JUMPOUT(x) __builtin_unreachable()
#define abs32(x) abs(x)

#define BYTEn(x, n) (*((_BYTE *)&(x) + (n)))
#define WORDn(x, n) (*((_WORD *)&(x) + (n)))
#define DWORDn(x, n) (*((_DWORD *)&(x) + (n)))
#define LOBYTE(x) BYTEn(x, 0)
#define HIBYTE(x) BYTEn(x, 1)
#define LOWORD(x) WORDn(x, 0)
#define HIWORD(x) WORDn(x, 1)
#define LODWORD(x) DWORDn(x, 0)
#define HIDWORD(x) DWORDn(x, 1)
#define SHIDWORD(x) (*((int *)&(x) + 1))
#define BYTE1(x) BYTEn(x, 1)
#define BYTE2(x) BYTEn(x, 2)
#define BYTE3(x) BYTEn(x, 3)
#define WORD1(x) WORDn(x, 1)
#define WORD2(x) WORDn(x, 2)
#define DWORD1(x) DWORDn(x, 1)
#define DWORD2(x) DWORDn(x, 2)
#define SDWORD1(x) (*((int *)&(x) + 1))
#define SDWORD2(x) (*((int *)&(x) + 2))
#define SWORD2(x) (*((short *)&(x) + 2))
#define _OWORD __int128

template <typename To, typename From> static inline To hx_cast(From value) {
  if constexpr (std::is_pointer_v<From> && std::is_integral_v<To>)
    return static_cast<To>(reinterpret_cast<uintptr_t>(value));
  else if constexpr (std::is_integral_v<From> && std::is_pointer_v<To>)
    return reinterpret_cast<To>(static_cast<uintptr_t>(value));
  else if constexpr (std::is_pointer_v<From> && std::is_pointer_v<To>)
    return reinterpret_cast<To>(value);
  else
    return static_cast<To>(value);
}

static inline __int128 hx_i128(uint64_t high, uint64_t low) {
  const unsigned __int128 bits = (static_cast<unsigned __int128>(high) << 64) | low;
  return static_cast<__int128>(bits);
}

#if defined(__aarch64__)
template <typename T> static inline T __ldaxr(T *p) {
  T value;
  if constexpr (sizeof(T) == 8) __asm__ volatile("ldaxr %0, [%1]" : "=r"(value) : "r"(p) : "memory");
  else if constexpr (sizeof(T) == 4) __asm__ volatile("ldaxr %w0, [%1]" : "=r"(value) : "r"(p) : "memory");
  else if constexpr (sizeof(T) == 2) __asm__ volatile("ldaxrh %w0, [%1]" : "=r"(value) : "r"(p) : "memory");
  else __asm__ volatile("ldaxrb %w0, [%1]" : "=r"(value) : "r"(p) : "memory");
  return value;
}
template <typename T> static inline int __stlxr(T value, T *p) {
  unsigned status;
  if constexpr (sizeof(T) == 8) __asm__ volatile("stlxr %w0, %1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  else if constexpr (sizeof(T) == 4) __asm__ volatile("stlxr %w0, %w1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  else if constexpr (sizeof(T) == 2) __asm__ volatile("stlxrh %w0, %w1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  else __asm__ volatile("stlxrb %w0, %w1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  return status;
}
template <typename T> static inline int __stxr(T value, T *p) {
  unsigned status;
  if constexpr (sizeof(T) == 8) __asm__ volatile("stxr %w0, %1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  else if constexpr (sizeof(T) == 4) __asm__ volatile("stxr %w0, %w1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  else if constexpr (sizeof(T) == 2) __asm__ volatile("stxrh %w0, %w1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  else __asm__ volatile("stxrb %w0, %w1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  return status;
}
static inline void __dmb(unsigned imm) { (void)imm; __asm__ volatile("dmb ish" ::: "memory"); }
#else
// Host-only syntax-test fallbacks; Android builds use the AArch64 asm above.
template <typename T> static inline T __ldaxr(T *p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }
template <typename T> static inline int __stlxr(T v, T *p) { __atomic_store_n(p, v, __ATOMIC_RELEASE); return 0; }
template <typename T> static inline int __stxr(T v, T *p) { __atomic_store_n(p, v, __ATOMIC_RELAXED); return 0; }
static inline void __dmb(unsigned) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
#endif

extern "C" void __cxa_finalize(void *);
extern "C" int __cxa_atexit(void (*)(void *), void *, void *);
extern "C" void __assert2(const char *, int, const char *, const char *);
