Tekken 6 Native Online — Linux prerelease

This is an experimental native Linux launcher/AppImage, not a prebuilt copy of
Tekken 6. It contains no disc image, extracted game data, generated game code,
save data, or prebuilt game executable.

1. Make the downloaded AppImage executable:
     chmod +x Tekken6NativeOnline-x86_64.AppImage
2. Launch it:
     ./Tekken6NativeOnline-x86_64.AppImage
3. In the launcher, choose “Build from my Xbox 360 copy…” and select your own
   supported USA/Europe Rev 1 Tekken 6 ISO or extracted disc directory.
4. Let the builder validate the copy, extract/codegen, and compile the game.

The AppImage stages its source workspace in:
  ~/.local/share/tekken6-native-online/workspace
(or $XDG_DATA_HOME/tekken6-native-online/workspace)

The writable workspace is necessary because code generation and CMake/Ninja
must create local build output. The game image and generated files stay on your
machine and are not uploaded by this workflow.

Building the game still requires the Linux build dependencies documented in
README.md (including Clang 18+, CMake, Ninja, Python 3, Git, Vulkan development
headers and audio/window-system development packages). The AppImage bundles the
Qt launcher runtime, not the full compiler toolchain.

Known limitations: this is a test prerelease. The full game build/play test is
not verified by CI. Linux controller binding UI, some window hide/restore
behavior, and Windows-only profiler/diagnostic paths remain incomplete.


The Linux AppImage bundles Qt's xcb, offscreen and Wayland platform plugins, the separate Wayland shell integrations (including xdg-shell) and wayland-egl client integration, Qt SVG image/icon plugins, and the OpenSSL shared libraries used by Qt TLS. The launcher is compiled with the published AppImage test version and uses this repository's public GitHub releases feed for Patch notes. The Linux test prereleases publish an AppImage rather than the Windows `tekken6_native_online_patch.zip`; download a newer AppImage release manually instead of expecting the Windows self-updater to replace it. CI verifies the plugin payload and checks the bundled Qt network/TLS runtime against the release feed.

Offline behavior: the public Linux build intentionally does not include the official lobby endpoint or its pinned certificate, so account login and online matchmaking are unavailable unless the service maintainer provides a valid endpoint and the exact pinned leaf certificate. Settings and local profile preferences remain available offline, and the game builder itself works locally. For a private/test service, pass the actual endpoint and pinned PEM leaf certificate from the service maintainer:

    T6_LOBBY_SERVER='tls://your-lobby-host:7700' T6_LOBBY_CERT='/path/to/pinned-lobby-cert.pem' ./Tekken6NativeOnline-x86_64.AppImage

TLS certificate pinning remains enabled. The game builder operates locally and does not itself require lobby connectivity.
