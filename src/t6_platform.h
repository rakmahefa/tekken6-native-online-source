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
#include <sys/mman.h>
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
#ifndef VK_BACK
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69
#define VK_ADD 0x6B
#define VK_SUBTRACT 0x6D
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
// Windows memory-protection constants used by the host snapshot code.
#ifndef PAGE_NOACCESS
#define PAGE_NOACCESS 0x01
#define PAGE_READONLY 0x02
#define PAGE_READWRITE 0x04
#define PAGE_WRITECOPY 0x08
#define PAGE_EXECUTE 0x10
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_WRITECOPY 0x80
#define PAGE_GUARD 0x100
#endif

inline int T6NativeProtection(DWORD flags)
{
   int protection = PROT_NONE;
   if (flags & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
      protection |= PROT_READ;
   if (flags & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
      protection |= PROT_WRITE;
   if (flags & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
      protection |= PROT_EXEC;
   return protection;
}

inline DWORD T6WindowsProtection(int protection)
{
   if ((protection & (PROT_READ | PROT_WRITE | PROT_EXEC)) == (PROT_READ | PROT_WRITE | PROT_EXEC))
      return PAGE_EXECUTE_READWRITE;
   if ((protection & (PROT_READ | PROT_EXEC)) == (PROT_READ | PROT_EXEC))
      return PAGE_EXECUTE_READ;
   if ((protection & PROT_EXEC) != 0 && (protection & PROT_READ) == 0)
      return PAGE_EXECUTE;
   if ((protection & (PROT_READ | PROT_WRITE)) == (PROT_READ | PROT_WRITE))
      return PAGE_READWRITE;
   if ((protection & PROT_READ) != 0)
      return PAGE_READONLY;
   return PAGE_NOACCESS;
}

// POSIX implementation of the VirtualProtect subset used when restoring
// guest-memory snapshots. The pre-change permission is inferred from maps so
// the same API call can be used to restore it afterward.
inline BOOL VirtualProtect(void* address, size_t size, DWORD new_protection, DWORD* old_protection)
{
   if (!address || size == 0) return FALSE;
   const long page_size_value = ::sysconf(_SC_PAGESIZE);
   if (page_size_value <= 0) return FALSE;
   const uintptr_t page_size = static_cast<uintptr_t>(page_size_value);
   const uintptr_t raw_start = reinterpret_cast<uintptr_t>(address);
   if (raw_start > UINTPTR_MAX - size) return FALSE;
   const uintptr_t start = raw_start & ~(page_size - 1);
   const uintptr_t raw_end = raw_start + size;
   if (raw_end > UINTPTR_MAX - (page_size - 1)) return FALSE;
   const uintptr_t end = (raw_end + page_size - 1) & ~(page_size - 1);

   if (old_protection) {
      int old_native = PROT_READ | PROT_WRITE;
      if (FILE* maps = std::fopen("/proc/self/maps", "r")) {
         char line[512];
         while (std::fgets(line, sizeof(line), maps)) {
            unsigned long map_start = 0, map_end = 0;
            char permissions[5] = {};
            if (std::sscanf(line, "%lx-%lx %4s", &map_start, &map_end, permissions) == 3 &&
                start >= map_start && start < map_end) {
               old_native = 0;
               if (permissions[0] == 'r') old_native |= PROT_READ;
               if (permissions[1] == 'w') old_native |= PROT_WRITE;
               if (permissions[2] == 'x') old_native |= PROT_EXEC;
               break;
            }
         }
         std::fclose(maps);
      }
      *old_protection = T6WindowsProtection(old_native);
   }
   return ::mprotect(reinterpret_cast<void*>(start), end - start, T6NativeProtection(new_protection)) == 0
       ? TRUE : FALSE;
}

#ifndef MEM_COMMIT
#define MEM_COMMIT 0x1000
#endif

struct MEMORY_BASIC_INFORMATION {
   void* BaseAddress = nullptr;
   void* AllocationBase = nullptr;
   DWORD AllocationProtect = 0;
   size_t RegionSize = 0;
   DWORD State = 0;
   DWORD Protect = PAGE_NOACCESS;
   DWORD Type = 0;
};

inline size_t VirtualQuery(const void* address, MEMORY_BASIC_INFORMATION* info, size_t info_size)
{
   if (!address || !info || info_size < sizeof(MEMORY_BASIC_INFORMATION))
      return 0;
   const uintptr_t target = reinterpret_cast<uintptr_t>(address);
   FILE* maps = std::fopen("/proc/self/maps", "r");
   if (!maps)
      return 0;
   char line[512];
   size_t result = 0;
   while (std::fgets(line, sizeof(line), maps)) {
      unsigned long map_start = 0, map_end = 0;
      char permissions[5] = {};
      if (std::sscanf(line, "%lx-%lx %4s", &map_start, &map_end, permissions) != 3)
         continue;
      if (target < map_start || target >= map_end)
         continue;
      int native = 0;
      if (permissions[0] == 'r') native |= PROT_READ;
      if (permissions[1] == 'w') native |= PROT_WRITE;
      if (permissions[2] == 'x') native |= PROT_EXEC;
      *info = MEMORY_BASIC_INFORMATION{};
      info->BaseAddress = reinterpret_cast<void*>(map_start);
      info->AllocationBase = info->BaseAddress;
      info->RegionSize = map_end - map_start;
      info->State = MEM_COMMIT;
      info->Protect = T6WindowsProtection(native);
      result = sizeof(MEMORY_BASIC_INFORMATION);
      break;
   }
   std::fclose(maps);
   return result;
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
