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
