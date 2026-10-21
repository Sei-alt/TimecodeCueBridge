#!/bin/bash
# Conservative uninstaller. Only removes the expected fixed app bundle and this script.
set -euo pipefail
APP="/Applications/Timecode Cue Bridge.app"
SCRIPT="/Applications/Uninstall Timecode Cue Bridge.command"
RECEIPT="org.timecodecuebridge.install"
printf 'Timecode Cue Bridge uninstaller\n\n'
if [[ -L "$APP" || -L "$SCRIPT" ]]; then
    echo "A symlink was found. Refusing to remove files automatically." >&2
    exit 1
fi
if [[ -d "$APP" ]]; then
    PLIST="$APP/Contents/Info.plist"
    [[ -f "$PLIST" ]] || { echo "Missing app Info.plist; refusing deletion" >&2; exit 1; }
    BUNDLE_ID=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$PLIST" 2>/dev/null || true)
    [[ "$BUNDLE_ID" == "org.timecodecuebridge.app" ]] || { echo "Unknown app bundle ID; refusing deletion" >&2; exit 1; }
fi
printf 'Remove only the application and installed uninstaller?\n'
printf 'Cue settings and logs will be PRESERVED. [type REMOVE]: '
read -r CONFIRM
[[ "$CONFIRM" == "REMOVE" ]] || { echo "Cancelled"; exit 0; }
if [[ -d "$APP" ]]; then sudo /bin/rm -R "$APP"; fi
if /usr/sbin/pkgutil --pkg-info "$RECEIPT" >/dev/null 2>&1; then
    sudo /usr/sbin/pkgutil --forget "$RECEIPT" >/dev/null
fi
if [[ -f "$SCRIPT" ]]; then sudo /bin/rm "$SCRIPT"; fi
printf '\nRemoved installed app components. User preferences and logs were kept.\n'
