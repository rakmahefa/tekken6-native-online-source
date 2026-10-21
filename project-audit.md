# Milestone 1 — native Linux port plan and audit

Date: 2026-10-21  
Base: `main` at `e61f084be5d062c96389b8e8cb9fc4dc5fedaed4`  
Implementation branch: `feature/native-linux-build`  
Milestone PR: [#1](https://github.com/rakmahefa/tekken6-native-online-source/pull/1)

## Objective and definition of done

Port the existing Windows-oriented application to a native Linux build without regressing Windows. Split work into four layers — GGPO/platform utilities, engine/runtime, player-side builder, and Qt launcher — and validate each layer independently on Ubuntu.

Milestone 1 is complete only when:
1. the isolated native components compile in clean CI;
2. the Qt launcher builds and links natively on Linux;
3. the Linux player-side builder passes syntax/argument checks and can configure/codegen/build from a user's own supported game copy;
4. all engine platform compatibility APIs compile with the SDK target, and the whole game executable links from a supplied game image;
5. a local smoke test can start the game and exercise the launcher/network/controller paths; Windows continues to build.

**Important validation boundary:** CI must never include a copyrighted Tekken 6 image or generated game-specific C++. The public checkout does not contain `Default.xex` or generated game sources, by design. Therefore the full game build and actual gameplay cannot be considered validated until a maintainer runs a private/local build from their own supported copy.

## Initial audit findings

### Engine and ReXGlue integration

- Root `CMakeLists.txt` builds a C++23 ReXGlue host, `tools/xdvdfs`, and the local GGPO fork.
- The ReXGlue SDK supports Linux with Vulkan; the application-level sources and current build assumptions were Windows-oriented.
- Engine utilities directly used Win32 clocks, thread/process IDs, memory-protection/query APIs, dynamic library discovery, CPUID intrinsics and crash-reporting APIs.
- The GGPO target unconditionally linked `ws2_32` and `winmm`, and its UDP/platform implementation used WinSock, Windows event handles and a Windows module entry point.

### Player-side builder

- `builder/build_native.py` and `builder/compile.json` are specifically for the Windows/MinGW package: `clang++.exe`, `xdvdfs.exe`, Windows system libraries and a GUI executable.
- The generated sources come from the player's own `Default.xex`; they are intentionally absent and ignored.
- A public CI build cannot generate or publish a complete game from the public checkout alone. The correct design is a local Linux builder that receives the player's own ISO/extracted directory, verifies the supported retail image, and keeps the game copy, generated files and build cache local.

### Launcher

- Qt Widgets/Network are portable, but launcher CMake used MSVC flags and linked Windows libraries.
- Source files directly used WinHTTP, WinMM/MCI, DXGI, Windows registry and process/job/mutex/window APIs, plus Windows-specific hidden-window and taskbar-flash behavior.
- The official deployment configuration, certificate and art files are intentionally not published. The native Linux launcher consequently still needs a clear public/default configuration policy before any official release.

## Porting plan

### Workstream A — GGPO and POSIX base layer

1. Add a POSIX-specific platform implementation while retaining the existing Windows implementation unchanged.
2. Keep Windows-only compiler defines/libraries and WinSock calls behind the Windows CMake condition.
3. Port UDP socket length/non-blocking/address handling, CRT compatibility helpers, timers, events/waits, diagnostics and process initialization.
4. Add a small Linux-only CMake test project independent of ReXGlue code generation and the private game image.

### Workstream B — engine/runtime

1. Introduce a small host platform boundary, leaving existing Windows APIs/behavior in the Windows branch.
2. Implement Linux clocks, process/thread identifiers, dynamic symbol/library lookup, atomic file replacement, CPUID and process-memory query/protection wrappers using POSIX APIs.
3. Add Linux crash output that preserves the operating system's core-dump behavior rather than pretending Windows stack walking exists.
4. Port system telemetry to Linux and make the Windows-only WinMM diagnostic/profiler code explicit.
5. Implement the game's Linux keyboard mapping through SDL, and configure ReXGlue for Vulkan.
6. Configure/build the entire target against the pinned SDK and test it using a maintainer-supplied supported game image. Do not upload generated game output.

### Workstream C — player-side builder

1. Add `builder/build_native_linux.py` without changing the existing Windows builder.
2. Accept a user's ISO or extracted folder; validate the supported USA/Europe Rev 1 `Default.xex` digest before extraction/compilation.
3. Compile the XDVDFS helper natively instead of relying on the Windows executable.
4. Apply project SDK patches, generate ReXGlue output, run the project's ordered netplay patch tools, reconfigure, and compile the Linux game.
5. Keep `native/`, generated sources, CMake/Ninja objects and the local game copy ignored by Git. Preserve unchanged manifest/config timestamps and let CMake/Ninja reuse object files across builds.
6. Add user-facing error output for missing toolchain/dependencies and unsupported images.

### Workstream D — Qt launcher

1. Gate Windows system headers and compiler/linker flags by platform.
2. Replace WinHTTP on Linux with Qt Network while retaining the Windows WinHTTP implementation.
3. Port system information, CPU checks, audio notification, per-instance locking and child-process lifetime handling to POSIX/Qt.
4. Pass Vulkan-compatible game arguments on Linux; remove D3D12/DXGI-only options from that path.
5. Use SDL keyboard/gamepad APIs where implemented and make incomplete Windows-only window/controller diagnostics explicit. Keep existing Windows behavior and UI code behind platform guards.
6. Compile and link the whole launcher on Ubuntu CI.

### Workstream E — CI verification and hand-off

1. Use separate Ubuntu jobs for GGPO/XDVDFS/engine platform-smoke tests and Qt launcher compilation, so failures are independently visible.
2. Syntax-check the Linux builder in CI without trying to use a game image.
3. Make the full private-image game build and actual gameplay a manual maintainer acceptance check.
4. After the native build is proven, add the release/AppImage workflows as the next milestone. Do not publish a package that is only a launcher shell or that implies gameplay is validated.

## Implementation progress

### Implemented on `feature/native-linux-build`

- Added `third_party/ggpo/src/lib/ggpo/platform_linux.h/.cpp` with POSIX socket/event/timer/process and CRT compatibility, while selecting the Windows implementation only for Windows builds.
- Updated root CMake to select platform-specific GGPO sources and link `ws2_32`/`winmm` only on Windows.
- Made GGPO UDP socket/address operations conditional for POSIX and replaced the Windows-only `DllMain` startup path with a Linux static initialization path.
- Added `src/t6_platform.h` for host clocks/thread IDs, dynamic module/symbol lookup, CPUID and Linux `VirtualQuery`/`VirtualProtect` equivalents; updated common engine helpers, crash handling, hang diagnostics and profiling guards.
- Added an SDL-based Linux keyboard input path preserving the `T6_NET_KEYS` layouts; Linux WinMM diagnostic and suspended-thread profiler sampling are reported as unsupported instead of being compiled as if they worked.
- Added `builder/build_native_linux.py` for an explicitly supplied local ISO/folder, checksum validation, native XDVDFS extraction, ReXGlue codegen/patch steps and cached CMake/Ninja compilation. Added ignored paths for local generated/game output.
- Added Linux conditionals to the Qt launcher: platform-specific CMake flags/libraries, Qt Network HTTP, POSIX audio fallback and system IDs, a lock-file single-instance mechanism, child process lifetime, Vulkan launch arguments and a Linux executable/build-state path.
- Added README build instructions and a two-job Ubuntu smoke workflow.
- Added the isolated platform smoke test for memory mapping APIs.

### CI observations so far

GitHub Actions is being used iteratively, and earlier runs are expected to be red because each exposed a missing platform detail. The log from run [39555772037](https://github.com/rakmahefa/tekken6-native-online-source/actions/runs/39555772037) showed that several GGPO Linux sources (including UDP, UDP protocol and the POSIX platform implementation) compiled. That run then found an extra preprocessor terminator in the engine test header and a missing Linux guard around `OutputDebugStringW`; both were corrected on the branch. The current replacement run must finish before this audit can claim the smoke checks are green.

CI has not been supplied with a retail game image; it intentionally cannot establish that the complete engine compiles or that the game can be played.

## Explicit remaining risks / known limitations

- **Full engine/game build is unverified.** The public CI smoke target checks GGPO, XDVDFS and isolated POSIX memory wrappers, not every SDK-generated engine source. A private, player-supplied image is needed for codegen and full linkage.
- **End-to-end gameplay is unverified.** No native Linux session/match has been tested.
- **Launcher controller tools are incomplete on Linux.** The game's SDL keyboard path exists, but the launcher's controller capture/test UI currently cannot enumerate/confirm Linux gamepads. It needs SDL controller integration and runtime tests.
- **Window management behavior differs.** Linux window hide/restore helpers are no-ops while the Vulkan window manages its own presentation; multi-window practice/parked behavior requires a real desktop test.
- **Windows-only profiling/diagnostics are not ported.** WinMM polling and suspended-thread profiling stay Windows-only and should remain labelled that way.
- **Official deployment settings/assets are absent.** The public launcher still has placeholder endpoint/configuration unless maintainers supply a supported public configuration. Do not leak private server credentials, TLS keys/certificates or non-public assets into CI artifacts.
- **Dependency/version floor needs a local run.** ReXGlue requests Clang 18+, CMake 3.25+ and Vulkan development dependencies. The player builder and deployment instructions must be verified against the actual pinned SDK revision.

## Current milestone status

**In progress — not release-ready.** The structural POSIX paths and isolated CI checks are implemented, but milestone acceptance still depends on clean CI results for the GGPO/platform and Qt launcher jobs, followed by a maintainer's full local build and gameplay test from their own supported game copy.

The next milestone after this one is the requested cached Linux release workflow and the separate GitHub Release/AppImage deployment workflow.
