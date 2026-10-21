#!/bin/bash
# Run from any directory on an Apple Silicon Mac with Command Line Tools and CMake.
set -euo pipefail
[[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]] || { echo 'Use an Apple Silicon Mac (native arm64 Terminal).' >&2; exit 2; }
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$ROOT/build-mac-arm64"
OUT="$ROOT/dist-mac"
for tool in cmake xcrun codesign hdiutil pkgbuild; do command -v "$tool" >/dev/null || { echo "Missing: $tool" >&2; exit 2; }; done
xcrun --sdk macosx --show-sdk-path
mkdir -p "$OUT"
# Command Line Tools contain the compiler and SDK needed by this application.
# Makefiles also work with full Xcode and avoid requiring its IDE generator.
GENERATOR="${TCB_CMAKE_GENERATOR:-Unix Makefiles}"
cmake -S "$ROOT" -B "$BUILD" -G "$GENERATOR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DBUILD_TESTING=ON -DTCB_BUILD_GUI=ON -DTCB_BUILD_INTEGRATION_TESTS=ON
cmake --build "$BUILD" --config Release --parallel "${TCB_BUILD_JOBS:-3}"
ctest --test-dir "$BUILD" -C Release --output-on-failure 2>&1 | tee "$OUT/ctest-mac.log"
APP="$BUILD/TimecodeCueBridge_artefacts/Release/Timecode Cue Bridge.app"
PLIST="$APP/Contents/Info.plist"
VERSION=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$PLIST")
EXECUTABLE=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$PLIST")
[[ "$(xcrun lipo -archs "$APP/Contents/MacOS/$EXECUTABLE")" == arm64 ]]
[[ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$PLIST")" == org.timecodecuebridge.app ]]
/usr/libexec/PlistBuddy -c 'Print :NSMicrophoneUsageDescription' "$PLIST"
mkdir -p "$APP/Contents/Resources/Licenses"
cp "$ROOT/LICENSE" "$ROOT/THIRD_PARTY_NOTICES.md" "$APP/Contents/Resources/Licenses/"
cp "$ROOT/licenses/AGPL-3.0.txt" "$ROOT/licenses/GPL-3.0.txt" "$ROOT/licenses/LGPL-3.0.txt" "$ROOT/licenses/JUCE-LICENSE.md" "$APP/Contents/Resources/Licenses/"
IDENTITY="${TCB_SIGN_IDENTITY:--}"
SIGN_ARGS=(--force --sign "$IDENTITY" --options runtime --entitlements "$ROOT/packaging/macos/entitlements.plist")
if [[ "$IDENTITY" != - ]]; then SIGN_ARGS+=(--timestamp); fi
codesign "${SIGN_ARGS[@]}" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"
# GUI construction/exit only; deliberately does not request microphone access.
"$APP/Contents/MacOS/$EXECUTABLE" --smoke-test > "$OUT/gui-smoke.log" 2>&1
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
/usr/bin/ditto "$APP" "$STAGE/Timecode Cue Bridge.app"
ln -s /Applications "$STAGE/Applications"
cp "$ROOT/README-macOS-JA.md" "$STAGE/はじめに.md"
cp "$ROOT/README-macOS.md" "$STAGE/Read Me.md"
DMG="$OUT/TimecodeCueBridge-${VERSION}-macOS-arm64-development.dmg"
hdiutil create -volname 'Timecode Cue Bridge' -srcfolder "$STAGE" -ov -format UDZO "$DMG"
hdiutil verify "$DMG"
bash "$ROOT/packaging/macos/build_pkg.sh" "$APP" "$OUT" "$VERSION"
/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$APP" "$OUT/TimecodeCueBridge-${VERSION}-macOS-arm64-app.zip"
(cd "$OUT" && shasum -a 256 ./*.dmg ./*.pkg ./*-app.zip > SHA256SUMS.txt)
# Loose build bundles are indexed as extra applications by macOS. Keep only the
# distributable archives by default; developers can opt in to retaining the app.
if [[ "${TCB_KEEP_APP_BUNDLE:-0}" != 1 ]]; then
  /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -u "$APP" || true
  /bin/rm -R "$APP"
fi
echo "Development artifacts and test logs: $OUT"
echo 'These artifacts are not notarized. Complete README-macOS.md acceptance checks on a Mac.'
