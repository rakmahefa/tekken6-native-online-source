#pragma once
// Tekken 6 Native Online launcher - The launcher's settings of the build and the text helper.

#include <QtWidgets>
#include <QtNetwork>
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <dwmapi.h>
#include <winhttp.h>
#include <dxgi1_6.h>
#include <intrin.h>
#include <mmsystem.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#else
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using DWORD = quint32;
using WORD = quint16;
using BYTE = quint8;
using LONG = qint32;
using HWND = void*;
using HANDLE = void*;
using HMODULE = void*;
using LPARAM = qintptr;
using WPARAM = quintptr;
using UINT = unsigned int;
using SHORT = qint16;
#ifndef VK_BACK
#define VK_BACK 0x08
#define VK_RETURN 0x0D
#define VK_ESCAPE 0x1B
#endif
using SOCKET = int;
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif
#ifndef closesocket
#define closesocket close
#endif
inline quint64 GetTickCount64()
{
  return static_cast<quint64>(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}
#endif
#include <algorithm>
#include <cmath>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

inline const char* const kVersion = "2.3.0";  // v2.0.1: start-of-match save stall, rematch card + same stage, loser's random stage (ranked), blur / vibration options
// The lobby server and the release repository of the official build (CMake T6_SERVER / T6_RELEASE_API, set by the
// untracked launcher/official.cmake); T6_LOBBY_SERVER / T6_UPDATE_API override them for local tests
#ifndef T6_SERVER
#define T6_SERVER "tls://lobby.example.com:7700"
#endif
#ifndef T6_RELEASE_API
#define T6_RELEASE_API L""
#endif
inline const char* const kServer = T6_SERVER;
inline constexpr int kProto = 2;
inline constexpr int kBlockedTab = 2;  // Profile -> BLOCKED
inline constexpr int kStatsTab = 6;
inline constexpr int kHistoryMax = 100;  // own match history (user 2026-10-18: 100 instead of 10; the server shows 100 too)    // Settings -> the developer's STATS (dev build only)

// releases of the package (GitHub; /releases/latest skips pre-releases); empty = no update check
inline const wchar_t* const kReleaseApi = T6_RELEASE_API;
inline const char* const kPatchAsset = "tekken6_native_online_patch.zip";
inline bool g_updateTest = false;

inline std::atomic<int> g_updateWorkerFailures{0};  // (update downloads ended by an exception: the button stays)
inline int g_instance = 1;
QString T(const char* en);
