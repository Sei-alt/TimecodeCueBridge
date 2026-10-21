# Timecode Cue Bridge 0.5.1

C++17/JUCE/libltc GUI: LTC audio -> scenes -> OSC/UDP and MIDI OUT.
Windows 11 x64 is the delivery target. Apple Silicon macOS CMake/packaging support is retained.
See README_JA.md for operation and BUILD_REPORT_JA.md for completed tests and remaining hardware checks.

## Changes in 0.5.1

- Each scene list row has its own Rearm button. It targets that row without changing the selection; disabled for pending, disabled or unapplied scenes. Recycled list components update their target row when scrolling.
- NSIS creates a per-user desktop shortcut using the embedded app icon on install/update and removes it on uninstall. The Windows lifecycle script checks its target, icon, removal and recreation.

## Changes in 0.5

- Reacquire LTC automatically after 250ms without a valid frame. `LtcStream` recreates the parser while lost, recovering from high-frequency pause noise that can leave libltc's speed tracker stuck. Both wall-clock and audio-time watchdogs are used.
- Live-path regression: actual audio callback -> FIFO -> worker -> decoder -> cues -> OSC UDP / MIDI sink, including repeated pauses, device restart and disarmed input monitoring.
- FIRED rows have a green background/stripe and a text badge; the scene list shows a completed/active count.
- Reset All rearms every scene; Rearm Scene clears only the selected scene. Stop/resume and rewind preserve history. Test Scene sends immediately and does not alter automatic history.
- Waveform/arrow icon embedded in the app, window and installers; small version label at the top right. Apple Silicon icon generation stays with JUCE/CMake.

## Existing features

- Scene list with Add (+), Remove (-), Duplicate, Undo Remove, Sort by Time.
- Separate scene name, enabled/disabled and output fields.
- English graphite UI with editable Hours/Minutes/Seconds/Frames fields: click to type, vertically drag the number to scrub (6px/step, Shift 24px/step), Enter/blur to commit, Esc to cancel; FPS-aware limits.
- Capture current incoming timecode into a scene.
- Per-scene OSC, MIDI or both; Note On, Note Off, CC, channel 1–16, data 0–127.
- Optional automatic Note Off (default 100ms, zero holds), dedicated scheduler, owned-note panic/cleanup on disarm/loss/config change/shutdown.
- MIDI output device selection/refresh. No MIDI input/MTC/virtual-port creation is included.
- Versioned JSON settings, v1/v2 migration (including commented-out cue rows) and backup of the old settings on first save.

## Same-PC MIDI routing

Select an existing virtual MIDI output in I/O Settings and enable that same port as a MIDI Input in Daslight. A separate virtual-port utility is required on Windows; no driver is bundled. See README_JA.md for the loopMIDI / Daslight 5 setup, mapping and Note Off behavior, with official links. Windows 11 + virtual port + Daslight has not been tested here.

## Native Windows build

Visual Studio 2022 Desktop C++, CMake 3.24+, NSIS are required.
The complete JUCE 8.0.12 and libltc v1.3.2 sources are bundled; there is no dependency source download at first configure.

```powershell
./packaging/windows/build.ps1
# Disposable clean Windows test account only:
./tests/windows-lifecycle.ps1 -Setup ./dist/TimecodeCueBridge-0.5.1-Win11-x64-Setup.exe
```

The script runs CMake, builds the GUI/test programs, runs CTest and packages using NSIS /WX.
The static release CRT removes the need for a separate Visual C++ Redistributable installer.
The installer registers the app per user and retains cues.json, events.log and unknown user files on uninstall.

## Linux -> Windows cross build

Requires Clang/lld 18+, Ninja, CMake, Python3, mingw-w64 windres (resources only), JUCE native build dependencies, and an xwin MSVC SDK tree.

```bash
xwin --accept-license splat --output /path/to/msvc-sysroot
export TCB_WINDOWS_SDK=/path/to/msvc-sysroot
cmake -S . -B build-cross -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/clang-msvc-x64.cmake
cmake --build build-cross --parallel 3
mkdir -p dist
makensis -WX -DAPP_BINARY="$PWD/build-cross/TimecodeCueBridge_artefacts/Release/Timecode Cue Bridge.exe" \
  -DSOURCE_DIR="$PWD" -DOUTPUT_DIR="$PWD/dist" packaging/windows/installer.nsi
```

The toolchain includes a Windows SDK case-insensitive header overlay, JUCE library filename aliases, Clang builtin header precedence and raw .res support.
It uses `_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH` for Clang 18 with the newer MSVC headers; the Microsoft SDK is not included in this source archive.
JUCE remains unmodified. MSVC native builds do not use this cross toolchain.

## macOS arm64 (not verified in this release)

```bash
cmake -S . -B build-mac -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-mac --config Release --parallel 4
ctest --test-dir build-mac -C Release --output-on-failure
bash packaging/macos/build_pkg.sh 'build-mac/TimecodeCueBridge_artefacts/Release/Timecode Cue Bridge.app' dist 0.5.1
```

## Tests

- `tcb_core_tests`: Release-safe cue checks, parser limits, timecode rules and MIDI packet byte boundaries.
- `tcb_integration`: generated libltc waveforms at 44.1/48/96kHz x 24/25/30fps -> shared decoder/engine -> actual OSC/UDP bytes, plus shared MIDI output service with an injected sink, note-off scheduler, panic/destruction, LTC -> MIDI, settings migration/roundtrip/rejection. Automatic reacquisition is tested at all nine sample-rate/FPS combinations, with silence/noise/DC, missing callbacks and counter wrap. The live capture callback/FIFO/thread path is now covered using injected PCM without opening hardware.
- `tcb_scene_tests`: message-thread JUCE controls for scene CRUD, names, numeric entry, invalid/empty text, Escape, real mouse events for vertical/Shift dragging, limits, pending-edit scene selection, FPS clamping and MIDI controls, fired progress and selective rearm. Requires a desktop display; registered in Windows/macOS CTest. On Linux run under Xvfb.
- `--smoke-test`: application GUI construction and normal shutdown after 1.5 seconds; never arms output.

MIDI sink tests do not prove physical port/driver behavior. Waveform tests do not exercise the physical AudioIODeviceCallback path. Real Windows hardware testing remains necessary where indicated in the build report.

Core only:
```bash
cmake -S . -B build-core -DTCB_BUILD_GUI=OFF -DTCB_BUILD_INTEGRATION_TESTS=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

## License

Application code and modifications: GNU AGPL v3 or later. JUCE: AGPL option. libltc: LGPL v3 or later.
See LICENSE, THIRD_PARTY_NOTICES.md, licenses and the complete third_party source trees for notices and rebuild/relink materials.
