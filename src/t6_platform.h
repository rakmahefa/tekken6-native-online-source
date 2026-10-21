#pragma once
// Platform boundary for the host-side netplay helpers. Keep Windows behavior
// on Windows; Linux uses POSIX clocks, process/thread IDs and libc utilities.
#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#include <mmsystem.h>
#include <intrin.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <strings.h>
#include <dlfcn.h>
#include <cpuid.h>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <thread>

using DWORD = uint32_t;
using LONG = int32_t;
using BOOL = int;
using UINT = unsigned int;
using SHORT = int16_t;
using HANDLE = void*;
using HMODULE = void*;
using HWND = void*;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef THREAD_PRIORITY_ABOVE_NORMAL
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#endif
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#ifndef _byteswap_ulong
#define _byteswap_ulong(v) __builtin_bswap32(static_cast<uint32_t>(v))
#endif
#ifndef _byteswap_ushort
#define _byteswap_ushort(v) __builtin_bswap16(static_cast<uint16_t>(v))
#endif
#ifndef __popcnt
#define __popcnt(v) __builtin_popcount(static_cast<unsigned int>(v))
#endif
#ifndef YieldProcessor
#define YieldProcessor() std::this_thread::yield()
#endif

struct LARGE_INTEGER { long long QuadPart = 0; };

inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* value)
{
   if (!value) return FALSE;
   value->QuadPart = 1000000000LL;
   return TRUE;
}
inline BOOL QueryPerformanceCounter(LARGE_INTEGER* value)
{
   if (!value) return FALSE;
   value->QuadPart = std::chrono::duration_cast<std::chrono::nanoseconds>(
       std::chrono::steady_clock::now().time_since_epoch()).count();
   return TRUE;
}
inline DWORD GetTickCount()
{
   return static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(
       std::chrono::steady_clock::now().time_since_epoch()).count());
}
inline DWORD GetCurrentThreadId()
{
   return static_cast<DWORD>(::syscall(SYS_gettid));
}
inline DWORD GetCurrentProcessId() { return static_cast<DWORD>(::getpid()); }
inline HANDLE GetCurrentThread() { return nullptr; }
inline HANDLE GetCurrentProcess() { return nullptr; }
inline BOOL SetThreadPriority(HANDLE, int) { return TRUE; }
inline BOOL SetProcessPriorityBoost(HANDLE, BOOL) { return TRUE; }
inline void Sleep(DWORD ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

inline int fopen_s(FILE** stream, const char* path, const char* mode)
{
   if (!stream) return EINVAL;
   *stream = std::fopen(path, mode);
   return *stream ? 0 : (errno ? errno : EIO);
}
inline int _putenv_s(const char* name, const char* value)
{
   if (!name || !*name) return EINVAL;
   return (value && *value) ? ::setenv(name, value, 1) : ::unsetenv(name);
}
inline int _stricmp(const char* a, const char* b)
{
   if (!a || !b) return a == b ? 0 : (a ? 1 : -1);
   return ::strcasecmp(a, b);
}
inline HMODULE GetModuleHandleW(const wchar_t* name)
{
   if (!name || !*name) return ::dlopen(nullptr, RTLD_LAZY);
   std::string module;
   for (const wchar_t* p = name; *p; ++p) {
      const wchar_t ch = *p;
      module.push_back(ch >= 0 && ch < 128 ? static_cast<char>(ch) : '?');
   }
   if (module.size() >= 4 && module.substr(module.size() - 4) == ".dll") {
      module.replace(module.size() - 4, 4, ".so");
      if (module.rfind("rex", 0) == 0) module.insert(0, "lib");
   }
   return ::dlopen(module.c_str(), RTLD_LAZY | RTLD_NOLOAD);
}
inline void* GetProcAddress(HMODULE module, const char* symbol)
{
   return ::dlsym(module ? module : RTLD_DEFAULT, symbol);
}
inline HMODULE LoadLibraryA(const char* name)
{
   return name ? ::dlopen(name, RTLD_LAZY) : nullptr;
}
inline BOOL MoveFileExA(const char* from, const char* to, DWORD)
{
   return ::rename(from, to) == 0 ? TRUE : FALSE;
}
#ifndef MOVEFILE_REPLACE_EXISTING
#define MOVEFILE_REPLACE_EXISTING 0x1
#endif

// MSVC-compatible CPUID output order used by the diagnostic / input code.
#ifdef __cpuid
#undef __cpuid
#endif
#define __cpuid(info, leaf) do { \
   unsigned int __a = 0, __b = 0, __c = 0, __d = 0; \
   __cpuid_count(static_cast<unsigned int>(leaf), 0, __a, __b, __c, __d); \
   (info)[0] = static_cast<int>(__a); (info)[1] = static_cast<int>(__b); \
   (info)[2] = static_cast<int>(__c); (info)[3] = static_cast<int>(__d); \
} while (0)
#endif
#endif // !_WIN32
