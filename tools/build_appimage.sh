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
        --exclude='./third_party/rexglue-sdk/tools/binutils/*.exe' \
        --exclude='./third_party/rexglue-sdk/tools/binutils/*.dll' \
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

# Install AppRun before linuxdeploy reads the desktop entry (Exec=AppRun).
# Keep our writable-workspace staging launcher as the final AppDir entry point.
install -m 0755 "$ROOT/packaging/appimage/AppRun" "$APPDIR/AppRun"

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

# linuxdeploy-plugin-qt currently selects xcb and common image-format plugins,
# but may omit optional platform plugins and Qt's SVG icon engine. Bundle each
# explicitly and ask linuxdeploy to resolve its transitive shared libraries.
QT_PLUGIN_ROOT="$(qmake6 -query QT_INSTALL_PLUGINS)"
WAYLAND_PLUGINS=("$QT_PLUGIN_ROOT"/platforms/libqwayland*.so)
SVG_ICON_PLUGIN="$QT_PLUGIN_ROOT/iconengines/libqsvgicon.so"
FOUND_WAYLAND=0
for plugin in "${WAYLAND_PLUGINS[@]}"; do
    [[ -f "$plugin" ]] || continue
    FOUND_WAYLAND=1
    relative="${plugin#"$QT_PLUGIN_ROOT/"}"
    install -D -m 0755 "$plugin" "$APPDIR/usr/plugins/$relative"
    APPIMAGE_EXTRACT_AND_RUN=1 "$LINUXDEPLOY_BIN" --appdir "$APPDIR" --library "$plugin"
done
if [[ "$FOUND_WAYLAND" -ne 1 ]]; then
    echo "Qt Wayland platform plugins were not installed at $QT_PLUGIN_ROOT/platforms" >&2
    exit 5
fi
if [[ ! -s "$SVG_ICON_PLUGIN" ]]; then
    echo "Qt SVG icon engine was not installed at $SVG_ICON_PLUGIN" >&2
    exit 5
fi
install -D -m 0755 "$SVG_ICON_PLUGIN" "$APPDIR/usr/plugins/iconengines/libqsvgicon.so"
APPIMAGE_EXTRACT_AND_RUN=1 "$LINUXDEPLOY_BIN" --appdir "$APPDIR" --library "$SVG_ICON_PLUGIN"

# Qt loads its OpenSSL backend and OpenSSL runtime libraries dynamically,
# so linuxdeploy cannot always infer the libssl/libcrypto dependencies.
# Bundle both SONAMEs explicitly rather than falling back to cert-only TLS.
mkdir -p "$APPDIR/usr/lib"
for SONAME in libssl.so.3 libcrypto.so.3; do
    RESOLVED="$(ldconfig -p | awk -v name="$SONAME" '$1 == name { print $NF; exit }')"
    if [[ -z "$RESOLVED" || ! -e "$RESOLVED" ]]; then
        echo "Required OpenSSL runtime library not found: $SONAME" >&2
        exit 4
    fi
    install -m 0755 "$(readlink -f "$RESOLVED")" "$APPDIR/usr/lib/$SONAME"
done

# Fail the package build if any plugin that the current launcher uses did not
# make it into the AppDir. These checks also protect future packaging changes.
test -s "$APPDIR/usr/plugins/platforms/libqxcb.so"
find "$APPDIR/usr/plugins/platforms" -maxdepth 1 -type f -name 'libqwayland*.so' -print -quit | grep -q .
test -s "$APPDIR/usr/plugins/imageformats/libqsvg.so"
test -s "$APPDIR/usr/plugins/iconengines/libqsvgicon.so"
test -s "$APPDIR/usr/plugins/tls/libqopensslbackend.so"
test -s "$APPDIR/usr/lib/libssl.so.3"
test -s "$APPDIR/usr/lib/libcrypto.so.3"

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
