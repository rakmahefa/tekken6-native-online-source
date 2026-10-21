#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${T6_APPIMAGE_VERSION:?Set T6_APPIMAGE_VERSION (for example 0.1.0-test.1)}"
SOURCE_VERSION="${T6_APPIMAGE_SOURCE_VERSION:-${VERSION}-${GITHUB_SHA:-local}}"
BUILD_ROOT="${T6_APPIMAGE_BUILD_DIR:-$ROOT/build/appimage}"
APPDIR="$BUILD_ROOT/AppDir"
SOURCE_STAGE="$BUILD_ROOT/workspace-stage"
OUT_DIR="${T6_APPIMAGE_OUT_DIR:-$ROOT/dist}"
LINUXDEPLOY_BIN="${LINUXDEPLOY_BIN:?Set LINUXDEPLOY_BIN to the linuxdeploy AppImage path}"
APPIMAGETOOL_BIN="${APPIMAGETOOL_BIN:?Set APPIMAGETOOL_BIN to the appimagetool AppImage path}"

if [[ ! "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+-test\.[0-9]+$ ]]; then
    echo "Expected a test-channel version such as 0.1.0-test.1, got: $VERSION" >&2
    exit 2
fi
[[ -x "$LINUXDEPLOY_BIN" ]] || { echo "linuxdeploy is not executable: $LINUXDEPLOY_BIN" >&2; exit 2; }
[[ -x "$APPIMAGETOOL_BIN" ]] || { echo "appimagetool is not executable: $APPIMAGETOOL_BIN" >&2; exit 2; }

rm -rf "$APPDIR" "$SOURCE_STAGE"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" \
    "$APPDIR/usr/share/icons/hicolor/scalable/apps" \
    "$APPDIR/usr/share/tekken6-native-online" "$SOURCE_STAGE" "$OUT_DIR"

echo "Building the native Qt launcher (no game image or generated game code is needed in CI)."
cmake -S "$ROOT/launcher" -B "$BUILD_ROOT/launcher" -G Ninja \
    -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_ROOT/launcher" --parallel 2
install -m 0755 "$BUILD_ROOT/launcher/Tekken6 Native Online" \
    "$APPDIR/usr/bin/tekken6-native-online"

# Package the source/build recipe the launcher invokes, but never the player's
# ISO, extracted data, game binary, generated sources, private settings or caches.
echo "Preparing a clean writable-workspace payload."
(
    cd "$ROOT"
    tar --exclude-vcs \
        --exclude='./.git' --exclude='./.github' \
        --exclude='./build' --exclude='./dist' --exclude='./native' \
        --exclude='./runs' --exclude='./game' --exclude='./user' --exclude='./user2' \
        --exclude='./toolchain' --exclude='./generated/default' \
        --exclude='./tests' --exclude='./builder/llvm-mingw' --exclude='./builder/python' \
        --exclude='./launcher/build*' \
        --exclude='./launcher/official.cmake' --exclude='./launcher/official.qrc' \
        --exclude='./launcher/ranks' --exclude='./launcher/art' --exclude='./launcher/tls' \
        --exclude='./tekken6_manifest.toml' --exclude='./tekken6_config.toml' \
        --exclude='*.iso' --exclude='*.ini' --exclude='*.log' --exclude='*.AppImage' \
        -cf - .
) | tar -xf - -C "$SOURCE_STAGE"

# The launcher deliberately follows GitHub prereleases only in this AppImage
# test copy, avoiding accidental test-channel updates of a normal install.
cat > "$SOURCE_STAGE/test_channel.txt" <<'EOF'
Linux AppImage test channel: this build intentionally follows prereleases.
EOF
tar -czf "$APPDIR/usr/share/tekken6-native-online/workspace.tar.gz" -C "$SOURCE_STAGE" .
printf '%s\n' "$SOURCE_VERSION" > "$APPDIR/usr/share/tekken6-native-online/source-version"
cp "$ROOT/packaging/appimage/README-linux-appimage.txt" \
   "$APPDIR/usr/share/tekken6-native-online/README-linux-appimage.txt"

# Fail closed if game media slipped into the portable source payload.
if tar -tzf "$APPDIR/usr/share/tekken6-native-online/workspace.tar.gz" | \
    grep -Eiq '(^|/)(default\.xex|data[^/]*\.bin|[^/]*\.iso)$'; then
    echo "Refusing to package an ISO, Default.xex, or extracted DATA*.BIN file." >&2
    exit 3
fi

cp "$ROOT/packaging/appimage/tekken6-native-online.desktop" "$APPDIR/tekken6-native-online.desktop"
cp "$ROOT/packaging/appimage/tekken6-native-online.desktop" \
   "$APPDIR/usr/share/applications/tekken6-native-online.desktop"
cp "$ROOT/packaging/appimage/tekken6-native-online.svg" \
   "$APPDIR/usr/share/icons/hicolor/scalable/apps/tekken6-native-online.svg"
ln -sfn usr/share/icons/hicolor/scalable/apps/tekken6-native-online.svg "$APPDIR/.DirIcon"

# linuxdeploy bundles shared libraries and Qt plugins; appimagetool is invoked
# separately so the custom AppRun (which stages the writable source workspace)
# is guaranteed to be the entry point embedded in the final image.
TOOL_DIR="$(dirname -- "$LINUXDEPLOY_BIN")"
PATH="$TOOL_DIR:$PATH" APPIMAGE_EXTRACT_AND_RUN=1 "$LINUXDEPLOY_BIN" \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/tekken6-native-online" \
    --desktop-file "$APPDIR/usr/share/applications/tekken6-native-online.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/tekken6-native-online.svg" \
    --plugin qt

install -m 0755 "$ROOT/packaging/appimage/AppRun" "$APPDIR/AppRun"
cp "$ROOT/packaging/appimage/tekken6-native-online.desktop" "$APPDIR/tekken6-native-online.desktop"
chmod 0755 "$APPDIR/usr/bin/tekken6-native-online"
rm -f "$OUT_DIR/Tekken6NativeOnline-x86_64.AppImage"
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$APPIMAGETOOL_BIN" \
    "$APPDIR" "$OUT_DIR/Tekken6NativeOnline-x86_64.AppImage"
chmod 0755 "$OUT_DIR/Tekken6NativeOnline-x86_64.AppImage"
cp "$ROOT/packaging/appimage/README-linux-appimage.txt" "$OUT_DIR/README-linux-appimage.txt"
(
    cd "$OUT_DIR"
    sha256sum Tekken6NativeOnline-x86_64.AppImage > SHA256SUMS
)

echo "Built: $OUT_DIR/Tekken6NativeOnline-x86_64.AppImage"
du -h "$OUT_DIR/Tekken6NativeOnline-x86_64.AppImage"
