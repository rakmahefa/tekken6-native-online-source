# Native Linux AppImage — experimental pre-release

This package is for testing the Linux launcher and the player-side builder. It is **not a verified gameplay release**.

## First launch

Make the AppImage executable and run it:

```sh
chmod +x Tekken6-Native-Online-linux-x86_64.AppImage
./Tekken6-Native-Online-linux-x86_64.AppImage
```

The launcher is bundled with its Qt runtime. To build the game from your own supported Tekken 6 Xbox 360 USA/Europe Rev 1 copy, the host still needs the native build dependencies listed in the repository README (Clang 18+, CMake, Ninja, Python 3, Git, Vulkan headers and Linux audio/windowing development packages). The builder validates the input before extracting it.

The first launch copies the open-source builder/project payload to `$XDG_DATA_HOME/tekken6-native-online` (normally `~/.local/share/tekken6-native-online`). The local game copy, generated game code, CMake cache, built executable and runtime libraries remain there and are not uploaded to GitHub. The AppImage is mounted read-only, so do not try to compile inside the mounted image.

## Milestone 2 test scope

Please report whether the AppImage starts, the game-copy picker opens, validation accepts your supported local copy, code generation/build completes, and the resulting native executable starts. Also report any missing shared libraries, audio/controller/window issues and the desktop environment used.

CI deliberately contains no retail game image and does not compile or distribute any game-specific generated C++. Matchmaking may not be configured in this public build because official endpoint/certificate/assets are not part of the public repository. Do not treat this pre-release as proof that online play works.

Include the workflow run URL and the AppImage SHA-256 when reporting a failure. Do not upload or commit your ISO, extracted disc contents, saves, generated C++ or private user data.
