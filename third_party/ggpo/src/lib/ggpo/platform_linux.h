/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net) - POSIX platform support.
 *
 * This project-specific port preserves the small Platform abstraction used
 * by the GGPO fork while keeping the Windows implementation unchanged.
 * ----------------------------------------------------------------------- */
#ifndef _GGPO_LINUX_H_
#define _GGPO_LINUX_H_

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/poll.h>
#include <strings.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <thread>
#include <netinet/in.h>

#include <algorithm>
#include <vector>

using SOCKET = int;
using HANDLE = int;
using BOOL = int;
using DWORD = unsigned long;
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif
#ifndef INFINITE
#define INFINITE 0xFFFFFFFFUL
#endif
#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0 0UL
#endif
#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT 258UL
#endif
#ifndef WSAEWOULDBLOCK
#define WSAEWOULDBLOCK EWOULDBLOCK
#endif

// WinSock compatibility spellings used by the legacy GGPO UDP code.
#ifndef closesocket
#define closesocket close
#endif
#ifndef WSAGetLastError
#define WSAGetLastError() (errno)
#endif
#ifndef ioctlsocket
#define ioctlsocket ioctl
#endif

inline int fopen_s(FILE** stream, const char* path, const char* mode)
{
   if (!stream) return EINVAL;
   *stream = std::fopen(path, mode);
   return *stream ? 0 : (errno ? errno : EIO);
}

inline int strcpy_s(char* destination, size_t destination_size, const char* source)
{
   if (!destination || destination_size == 0 || !source) return EINVAL;
   const size_t length = std::strlen(source);
   if (length >= destination_size) {
      destination[0] = '\0';
      return ERANGE;
   }
   std::memcpy(destination, source, length + 1);
   return 0;
}

template <size_t N>
inline int strcpy_s(char (&destination)[N], const char* source)
{
   return strcpy_s(destination, N, source);
}

inline void OutputDebugStringA(const char* message)
{
   if (message) std::fputs(message, stderr);
}

inline void DebugBreak()
{
   std::raise(SIGTRAP);
}

inline BOOL CreateDirectoryA(const char* path, void*)
{
   return path && ::mkdir(path, 0755) == 0 ? 1 : 0;
}

inline int sprintf_s(char* destination, size_t destination_size, const char* format, ...)
{
   if (!destination || destination_size == 0 || !format) return EINVAL;
   va_list args;
   va_start(args, format);
   const int written = std::vsnprintf(destination, destination_size, format, args);
   va_end(args);
   if (written < 0 || static_cast<size_t>(written) >= destination_size) {
      destination[0] = '\0';
      return ERANGE;
   }
   return written;
}

inline int vsprintf_s(char* destination, size_t destination_size, const char* format, va_list args)
{
   if (!destination || destination_size == 0 || !format) return EINVAL;
   const int written = std::vsnprintf(destination, destination_size, format, args);
   if (written < 0 || static_cast<size_t>(written) >= destination_size) {
      destination[0] = '\0';
      return ERANGE;
   }
   return written;
}

// The old Poll abstraction waits on Windows event handles. eventfd gives us
// equivalent waitable descriptors without requiring a thread or a busy loop.
inline HANDLE CreateEvent(void*, BOOL, BOOL initially_signaled, const char*)
{
   const int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
   if (fd == -1) return -1;
   if (initially_signaled) {
      const uint64_t one = 1;
      if (write(fd, &one, sizeof(one)) != sizeof(one)) {
         close(fd);
         return -1;
      }
   }
   return fd;
}

inline BOOL SetEvent(HANDLE fd)
{
   const uint64_t one = 1;
   return fd >= 0 && write(fd, &one, sizeof(one)) == sizeof(one);
}

inline BOOL ResetEvent(HANDLE fd)
{
   uint64_t value;
   while (fd >= 0 && read(fd, &value, sizeof(value)) == sizeof(value)) {}
   return fd >= 0;
}

inline void Sleep(DWORD milliseconds)
{
   std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

inline void YieldProcessor()
{
   std::this_thread::yield();
}

inline BOOL CloseHandle(HANDLE fd)
{
   return fd >= 0 ? close(fd) == 0 : 0;
}

inline DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL wait_all, DWORD timeout_ms)
{
   if (count == 0 || !handles) return WAIT_TIMEOUT;
   std::vector<pollfd> descriptors;
   descriptors.reserve(count);
   for (DWORD i = 0; i < count; ++i) {
      descriptors.push_back(pollfd{handles[i], POLLIN, 0});
   }
   const int timeout = timeout_ms == INFINITE
       ? -1
       : static_cast<int>(std::min<DWORD>(timeout_ms, static_cast<DWORD>(2147483647)));
   const int ready = ::poll(descriptors.data(), descriptors.size(), timeout);
   if (ready <= 0) return WAIT_TIMEOUT;
   if (wait_all) {
      for (DWORD i = 0; i < count; ++i) {
         if (!(descriptors[i].revents & (POLLIN | POLLERR | POLLHUP))) return WAIT_TIMEOUT;
      }
      return WAIT_OBJECT_0;
   }
   for (DWORD i = 0; i < count; ++i) {
      if (descriptors[i].revents & (POLLIN | POLLERR | POLLHUP)) return WAIT_OBJECT_0 + i;
   }
   return WAIT_TIMEOUT;
}

class Platform {
public:
   using ProcessID = pid_t;

   static ProcessID GetProcessID() { return getpid(); }
   static void AssertFailed(char* message)
   {
      FILE* file = std::fopen("ggpo_assert.txt", "a");
      if (file) {
         std::fputs(message ? message : "GGPO assertion failed", file);
         std::fputc('\n', file);
         std::fclose(file);
      }
   }
   static uint32 GetCurrentTimeMS()
   {
      using namespace std::chrono;
      return static_cast<uint32>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
   }
   static int GetConfigInt(const char* name);
   static bool GetConfigBool(const char* name);
};

#endif // _GGPO_LINUX_H_
