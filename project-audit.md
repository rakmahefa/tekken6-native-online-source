# Project audit — native Linux build and AppImage

Date: 2026-10-21  
Base branch: `main` (`e61f084be5d062c96389b8e8cb9fc4dc5fedaed4`)  
Work branch: `feature/native-linux-build`

## Executive summary

**Status: blocked for a truthful, working Linux release pipeline until native Linux support is implemented.** The repository has CMake and its ReXGlue SDK submodule supports Linux/Vulkan, but the application-specific game target, GGPO fork, launcher and player-side builder remain Windows-oriented. Adding packaging YAML alone would result in a red workflow or, worse, an artifact that looks like a Linux release but cannot launch the game.

The repository explicitly does not contain the game image or generated game sources. The player is expected to provide their own supported Tekken 6 Xbox 360 copy; generated code and extracted game data are intentionally excluded from version control. CI must not embed a copyrighted game image, generated game code, game data or save data in a public artifact.

## Repository layout inspected

- `CMakeLists.txt`: game/runtime target, GGPO and `tools/xdvdfs`.
- `generated/rexglue.cmake`: generated SDK integration and `tekken6_codegen` custom target.
- `third_party/rexglue-sdk`: Git submodule; upstream SDK supports Linux x86-64 and Vulkan.
- `third_party/ggpo`: project GGPO fork, containing a Windows platform implementation.
- `launcher/CMakeLists.txt`, `launcher/src/`: Qt launcher.
- `builder/build_native.py`, `builder/compile.json`: player's game builder and compiler/link configuration.
- `.gitignore`: intentionally excludes `game/`, `generated/default/*` (except `.gitkeep`), `build/`, `out/` and package/runtime folders.
- No existing `.github/workflows/` directory was found on `main`.

## Findings and blockers

### P0 — game target is not yet a Linux build

The root CMake configuration unconditionally links GGPO with `ws2_32` and `winmm`. Those are Windows libraries. The vendored GGPO implementation includes Windows-specific networking/platform code, and no POSIX platform implementation is present in the inspected GGPO tree. A Linux job would need a supported socket/platform layer and conditional libraries before compilation/linking can succeed.

### P0 — the player-side builder emits a Windows executable

`builder/compile.json` is configured for a Windows GUI executable: it includes `-municode`, `-mwindows`, `-lws2_32`, `-lkernel32`, `-luser32`, `-ldxgi`, `-lwinmm` and other Windows system libraries. `builder/build_native.py` expects `clang++.exe`, `xdvdfs.exe`, Windows paths and Windows runtime assets. This is not a Linux player build pipeline; it needs a Linux toolchain/runtime configuration and Linux-aware build logic.

### P0 — launcher is Windows-specific

`launcher/CMakeLists.txt` uses MSVC flags (`/utf-8`, `/W3`, `/Zi`, `/DEBUG`) and links Windows libraries (`dwmapi`, `winhttp`, `dxgi`, `winmm`, `dbghelp`, `ws2_32`). Launcher source files directly call Win32 APIs, including process/job APIs, DXGI GPU enumeration, MCI/PlaySound, registry-based machine identity, crash handlers and mutex APIs. Qt being cross-platform does not make these implementation files portable on its own.

### P0 — the game binary cannot be produced from the public checkout alone

The generated source directory is intentionally absent and ignored by Git. The SDK codegen target consumes a game manifest that references `Default.xex`; the README documents that a supported retail copy must be supplied locally. A public Actions workflow cannot assume this input exists and should not publish game-derived code/data. The pipeline must build only distributable, non-game-specific components in CI, while the player-side Linux builder accepts their own copy locally.

### P1 — the public launcher configuration is incomplete

The README documents that `launcher/official.cmake` and `launcher/official.qrc` are not published. Without them, the launcher uses placeholder server/release settings and does not include official art/certificate assets. Even after Linux compilation support is added, a release needs a deliberate policy for public configuration and server credentials/certificates.

### P1 — no packaging or CI conventions exist yet

No existing workflows were found. There is no established Linux install tree, AppDir layout, desktop entry, AppImage icon, or stable artifact naming/version convention to reuse.

## Recommended implementation plan

### Phase 1 — make a real Linux target buildable (required before release CI)

1. Add platform-aware CMake setup. Keep Windows build behavior unchanged; choose POSIX libraries only on Linux.
2. Add/enable a Linux GGPO networking/platform implementation and check the game/runtime's OS-specific code. Confirm ReXGlue target setup uses Vulkan rather than D3D12 on Linux.
3. Create Linux build configuration for the player-side game builder, including the right compiler, XEX extraction executable, SDK/runtime outputs, shared-library staging, and incremental object/cache strategy. The builder must work on the player's machine from their own game copy.
4. Port or isolate launcher Win32 functionality behind platform abstractions. Provide Linux equivalents for process lifetime, instance locking, audio, GPU information, machine identity, crash reporting and process launching. Replace MSVC-only CMake flags under Linux.
5. Define whether release CI ships the launcher, builder, supporting native tools, and SDK/runtime only, with no generated Tekken 6 code or data.
6. Validate the Linux target on a clean Ubuntu runner before enabling publication.

### Phase 2 — build workflow (after Phase 1)

Create `.github/workflows/build-linux.yml` with least-privilege read permissions, Ubuntu x86-64, recursive submodule checkout, pinned build dependencies, CMake/Ninja/Clang, and a Linux/Vulkan configuration. Use `ccache` plus `actions/cache` for compiler objects and safe CMake/SDK build outputs; use dependency- and toolchain-aware cache keys with restore prefixes. Build and test the actual native targets, stage a versioned `dist/linux/` release directory, build an AppImage from the same staged application, generate SHA-256 checksums, and upload one artifact containing both the Linux release archive and AppImage. Do not package `game/`, generated game sources, user saves, or a game image.

### Phase 3 — release workflow (after Phase 2)

Create a separate `.github/workflows/release-linux.yml` that publishes only after a successful build artifact for a version tag (recommended `v*`) or an explicit dispatch tied to a successful run. Give this workflow `contents: write` only. Download the artifact from the relevant workflow run, verify that both expected assets and checksums exist, then create/update the GitHub Release and attach the Linux archive, AppImage and checksums. Fail closed if the artifact is missing or the build run failed. Avoid executing untrusted pull-request code with release permissions.

### Phase 4 — acceptance criteria

- Clean Ubuntu runner compiles the supported native Linux targets; no Win32 linker flags or DLL-only dependencies remain in Linux targets.
- CI artifact contains both the Linux release archive and executable `.AppImage`.
- The AppImage launches on a clean supported Ubuntu environment and reports any missing runtime prerequisites clearly.
- The packaged player-side builder accepts a user-supplied supported game copy; CI artifacts contain no game image, extracted game data, generated game-specific source or saves.
- Cache restore is visible in logs and does not reuse incompatible outputs across different toolchains/platforms.
- A version tag publishes only the successful artifact, with checksums and reproducible asset names.
- Windows builds continue to work unchanged.

## Decision

Do not pretend that workflow YAML alone can make the existing Windows-oriented application native on Linux. The release/build workflows should be added as production workflows only after Phase 1 makes the Linux targets real and CI can demonstrate a successful build. Until then, an AppImage or Linux release asset would be misleading.
