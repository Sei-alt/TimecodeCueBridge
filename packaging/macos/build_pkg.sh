#!/bin/bash
set -euo pipefail
if [[ "$(uname -s)" != "Darwin" ]]; then echo "Run on macOS" >&2; exit 2; fi
APP_PATH="${1:?Usage: build_pkg.sh APP_PATH OUTPUT_DIR [VERSION]}"
OUTPUT_DIR="${2:?Missing output dir}"
VERSION="${3:-0.5.1}"
[[ -d "$APP_PATH" ]] || { echo "App missing: $APP_PATH" >&2; exit 3; }
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/root/Applications"
ditto "$APP_PATH" "$TMP/root/Applications/Timecode Cue Bridge.app"
install -m 755 "$(dirname "$0")/Uninstall Timecode Cue Bridge.command" "$TMP/root/Applications/Uninstall Timecode Cue Bridge.command"
# This creates an UNSIGNED development package. Sign + notarize before public distribution.
pkgbuild --root "$TMP/root" \
  --install-location / \
  --identifier org.timecodecuebridge.install \
  --version "$VERSION" \
  "$OUTPUT_DIR/TimecodeCueBridge-${VERSION}-macOS-arm64-unsigned.pkg"
