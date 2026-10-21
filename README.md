# Tekken 6 Native Online

A native PC port of Tekken 6 (Xbox 360) built with the [ReXGlue](https://github.com/rexglue/rexglue-sdk) static
recompiler, with rollback netcode (a GGPO fork) and a launcher for ranked / casual matchmaking, lobbies and practice.

**No game code or data is in this repository.** The player builds the game on their own PC from their own copy of
Tekken 6 (USA / Europe, Rev 1, Xbox 360): the launcher's builder translates the disc's `Default.xex` to C++, applies the
netplay patches and compiles it. Generated code, the game image, extracted data and save data never belong here.

## Layout

- `src/` - the game side: netplay (rollback snapshots, input, sound boundary, determinism), launcher status /
  commands, HUD name plates, practice frame data, internal resolution.
- `launcher/` - the Qt launcher (matchmaking UI, game builder front end, controls, updates).
- `builder/` - `build_native.py`, the build the player's package runs (codegen, patch tools, compile, link).
- `third_party/ggpo/` - GGPO with the project's changes (MIT, see its LICENSE).
- `third_party/rexglue-sdk/` - the SDK (submodule); `patches/rexglue/` holds the project's SDK changes.
- `tools/` - the patch tools of the generated code and development / test scripts.

## Building

1. `git submodule update --init --recursive`, then `python tools/apply_rexglue_patches.py`.
2. Put your own extracted game into `game/` (or let the launcher's builder do it from an ISO).
3. Game: `build_x360.cmd` (clang + MSVC ABI) or `build_mingw.cmd` (llvm-mingw, the package build).
4. Launcher: `launcher\build.bat` (Qt 6, MSVC).

The lobby server is not part of this repository. The official build's server address, release repository, pinned
server certificate, rank pictures and launcher art come from `launcher/official.cmake` and `launcher/official.qrc`,
which are not published; without them the launcher builds with placeholders (`T6_SERVER`, `T6_RELEASE_API` in
`launcher/CMakeLists.txt`) and draws text instead of the art.

## Native Linux build (experimental)

The native Linux port is under active development. The Qt launcher and the standalone GGPO/XDVDFS/host-platform smoke targets have Linux build paths; full gameplay still requires a local build from a player's own supported Tekken 6 Xbox 360 copy. No game image, game data, saves, or generated game-specific C++ is shipped.

### Build the launcher

On Ubuntu 24.04, install the native compiler and Qt development headers, then build:

```sh
sudo apt update
sudo apt install --yes clang cmake ninja-build qt6-base-dev
cmake -S launcher -B build/launcher-linux -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build build/launcher-linux --parallel
```

### Build the game from your own copy

The SDK needs Clang 18 or newer, CMake 3.25 or newer, Ninja, Python 3, Git, Vulkan development headers and Linux audio/window-system dependencies. On Ubuntu 24.04 these can be installed with:

```sh
sudo apt update
sudo apt install --yes clang cmake ninja-build python3 git build-essential \
  libvulkan-dev libgtk-3-dev libx11-xcb-dev libxss-dev \
  libwayland-dev libwayland-bin wayland-protocols libxkbcommon-dev \
  libdecor-0-dev libasound2-dev libpulse-dev libpipewire-0.3-dev
git submodule update --init --recursive
```

Then pass either an ISO image or the directory containing `Default.xex` and the adjacent `DATA*.BIN` files:

```sh
python3 builder/build_native_linux.py --check /path/to/your/Tekken6.iso
python3 builder/build_native_linux.py /path/to/your/Tekken6.iso
# or use the extracted game directory:
python3 builder/build_native_linux.py /path/to/extracted/Tekken6
```

The builder validates the supported USA/Europe Rev 1 `Default.xex` SHA-256 before extracting or compiling it. It stores the private game copy, generated C++, CMake/Ninja cache, SDK outputs and resulting executable under the ignored `native/` and `generated/default/` paths. Subsequent builds reuse CMake/Ninja's object cache. Never commit those local files.

**Current port limitations:** CI does not build or validate the complete game because the supported `Default.xex` is intentionally not present in the public repository. A Linux SDL keyboard mapping is now implemented but still needs a full game/play test. Windows-only input diagnostics and suspended-thread profiler sampling are not ported. The launcher’s controller-capture/test UI does not yet enumerate Linux controllers, and Linux window hide/restore helpers are currently no-ops; SDL is the intended gamepad/input path and Vulkan is used for Linux GPU selection. Treat Linux support as experimental until the entire game builds from a local copy and real gameplay is tested.

## Credits

- Based on [mahmoud-mess/tekken6-recomp](https://github.com/mahmoud-mess/tekken6-recomp) (the original ReXGlue setup
  of the game, the SDK patches, the internal resolution and speed-up work), used with the author's permission.
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) (BSD 3-Clause, derived from Xenia).
- [GGPO](https://github.com/pond3r/ggpo) (MIT).
- Bai Jamjuree font (SIL Open Font License, `launcher/fonts/OFL-BaiJamjuree.txt`), Material Symbols icons (Apache 2.0,
  `launcher/icons/LICENSE.txt`).

Tekken is a trademark of Bandai Namco Entertainment. This project is not affiliated with or endorsed by Bandai Namco.
