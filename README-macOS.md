# Timecode Cue Bridge — macOS 0.6.1

Apple Silicon (arm64); deployment target macOS 11.0 or later. This native Apple Silicon version is included in the official Windows/macOS combined release dated 22 October 2026. The included Windows version is 0.5.1 and the Mac version is 0.6.1. Native test results and remaining hardware checks are recorded in VERIFICATION-macOS-JA.md. The minimum OS target is a build setting; macOS 11 has not been tested.

## Build on a Mac

Install Apple Command Line Tools (`xcode-select --install`) or full Xcode, and CMake 3.24 or newer. Full Xcode is optional. Ensure `cmake` and `ctest` are on PATH. Use a native Apple Silicon Terminal. JUCE 8.0.12 and libltc 1.3.2 sources are bundled; dependency downloads are unnecessary.

From the extracted source directory:

```bash
bash packaging/macos/build.sh
```

The script configures an arm64 Release build with CMake/Unix Makefiles, builds the GUI and tests, runs CTest (cue engine, generated LTC through OSC/UDP, scene UI, real CoreMIDI virtual routing), checks the bundle ID, microphone usage description and arm64 executable, signs the app ad hoc, checks its signature, and runs a GUI-only startup/exit smoke test. It then creates `dist-mac/` containing an app ZIP, DMG, unsigned PKG, test logs and SHA256SUMS.txt. A failing build/test stops packaging. `--smoke-test` does not open audio or request microphone access; it is not an LTC hardware test.

The manual GitHub Actions workflow runs the same script on `macos-15`. It has been supplied but **not executed** for this delivery. No automatic Windows rebuild is configured in this Mac snapshot.

## Install and first launch

After a successful native build, open the DMG and drag **Timecode Cue Bridge.app** to **Applications**. Alternatively use the PKG; it installs the app and a conservative uninstaller in `/Applications`. Do not install both formats for the same test.

Launch from Applications. Allow microphone access, which is required for the LTC audio input, including external audio interfaces. If denied, allow Timecode Cue Bridge in System Settings → Privacy & Security → Microphone, then restart the app. The audio selector remains disabled until permission is granted.

If macOS asks for local network access, allow it when sending OSC to another device on your LAN.

In **I/O Settings**, select the LTC input device/channel and the matching 24/25/30 fps non-drop frame rate. Select the OSC destination and/or MIDI output. Use **Save & Apply**, start LTC, verify **LOCKED**, then **Arm**. Startup remains disarmed. Scene editing, typed/dragged time fields, FIRED progress, row Rearm, Reset All and MIDI Panic retain the Windows behavior. A Rearm button resets only that scene's fired state; it does not immediately send a cue.

## MIDI within one Mac

1. Open **Audio MIDI Setup** → **Window → Show MIDI Studio**.
2. Open **IAC Driver**, enable **Device is online**, and add/enable a bus.
3. In this app, select **I/O Settings → Refresh**, then choose the IAC bus as MIDI output.
4. Select the same IAC bus as the MIDI input in the receiving application, such as a compatible Mac installation of Daslight. Configure its MIDI mapping.
5. Save & Apply, then use **Test Scene** to verify Note/CC reception before arming LTC.

Avoid routing the receiver's MIDI output back into the same input bus. The automated CoreMIDI test creates a temporary unique virtual destination; it does not change IAC configuration or require a physical MIDI device. On 22 October 2026, the user confirmed MIDI routing from Timecode Cue Bridge 0.6.1 through the IAC Driver to Daslight 5 on an M1 MacBook Air. Individual MIDI message types were not specified.

## Data and uninstall

Settings and logs are stored at:

```text
~/Library/Application Support/TimecodeCueBridge/cues.json
~/Library/Application Support/TimecodeCueBridge/events.log
```

**Data Folder** opens this location. The Windows JSON scene format is unchanged; copying a Windows cues.json is possible, but audio device and MIDI identifiers differ, so choose Mac devices and Save & Apply. No automatic migration from an earlier experimental `~/Library/TimecodeCueBridge` folder is performed: copy cues.json if you have such a build, leaving the original intact.

Quit the app before uninstalling. For DMG installation, move the app from Applications to Trash. For PKG installation, run **Uninstall Timecode Cue Bridge.command** and type `REMOVE`. It checks the expected bundle ID, rejects symlinks, and removes only the installed app, its own script and package receipt. It does not remove the data directory. Reinstallation should restore saved scenes; verify this on the Mac before using the app in production.

## Source and license notices

The packaging script includes license texts and third-party notices inside the app's `Contents/Resources/Licenses` directory. Distribute the matching complete source ZIP with the binaries under the repository's licensing terms; see LICENSE and THIRD_PARTY_NOTICES.md.

## Signing and public distribution

This official release is ad-hoc signed and **not notarized**. Official release status does not imply Developer ID signing or Apple notarization. No Apple certificate or account was available in this environment. Setting `TCB_SIGN_IDENTITY` to an installed Developer ID Application identity makes the build script sign the app with a timestamp and Hardened Runtime audio-input entitlement; it still does not notarize the app or sign the installer.

If Developer ID signing and notarization are added later, sign the app and PKG, submit the final distribution through `xcrun notarytool`, staple the accepted ticket and verify Gatekeeper on a separate Mac. Do not describe ad-hoc signatures or a successful `codesign --verify` as notarization. Signing/notarization must be tested with microphone access enabled.

## Manual acceptance checklist

- Native build and all four CTest entries pass; bundle is arm64, version 0.6.1, icon visible in Finder/Dock.
- Permission first request, grant, deny, later grant/restart, and closing the app while the dialog is open.
- External LTC at 24/25/30 fps and 44.1/48 kHz; stop/resume repeatedly, seek and rewind, disconnect/reconnect input.
- Actual OSC receiver sees exact address/value once per armed scene; no automatic output while disarmed.
- IAC → receiving app: Note On/Off, CC, gate duration, Panic/disarm/input-loss cleanup.
- FIRED appearance, row Rearm on a non-selected scene, Reset All, name/time editing and save/relaunch.
- DMG copy/launch/removal and PKG install/uninstall/reinstall; cue settings and logs survive.
- Run on intended macOS versions and verify layout on a MacBook display, sleep/wake and audio device changes.
- Developer ID signing, notarization and Gatekeeper on a fresh Mac before distribution.

References: [Apple microphone authorization](https://developer.apple.com/documentation/avfoundation/avcapturedevice/requestaccess(for:completionhandler:)), [Apple IAC routing](https://support.apple.com/guide/audio-midi-setup/ams1013/mac), [Apple Hardened Runtime](https://developer.apple.com/documentation/security/hardened-runtime), [Apple notarization](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution), [GitHub runner reference](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).

## MacBook window layout

The window initially fits the usable display area. On shorter displays the Scenes tab scrolls vertically so MIDI value and gate controls remain accessible, while Save & Apply, Arm, Test Scene and Panic stay visible. The minimum window size is 1040 × 650.

For a GUI-only screenshot without opening audio or requesting microphone access, run the application executable with `--smoke-test --snapshot /absolute/path/preview.png`. The app exits automatically.

## Build artifact cleanup

After successful packaging, the build script unregisters and removes its loose build .app to keep test copies out of the macOS app list. The DMG, PKG and app ZIP remain in dist-mac. Developers who need the loose app can set `TCB_KEEP_APP_BUNDLE=1`. Eject the DMG after copying the app to Applications.
