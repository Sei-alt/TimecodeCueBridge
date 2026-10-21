Timecode Cue Bridge 0.6.1
=========================
Application changes are supplied under AGPL-3.0-or-later (see LICENSE).
This distribution uses JUCE 8.0.12 under its AGPLv3 option. It does not require a commercial JUCE license for use under those terms.
JUCE copyright: Raw Material Software Limited; see licenses/JUCE-LICENSE.md and third_party/JUCE/LICENSE.md for complete notices, including embedded dependencies.
libltc v1.3.2, copyright Robin Gareus, Jan Weiss and contributors, LGPL-3.0-or-later. Full license in licenses/LGPL-3.0.txt; sources in third_party/libltc.
The companion Source.zip includes the complete application and exact third-party source trees, build scripts and tests. It is provided together with the installer to enable rebuilding/relinking with modified libltc. No signing or installation key is required.
Windows cross builds use Clang/lld and the Microsoft MSVC/Windows SDK toolchain with a statically linked release CRT. Windows system libraries and the Microsoft build prerequisites retain their own licenses. The SDK itself is not included. MinGW windres is used only to compile Windows resources; no MinGW runtime is linked into the application.
NSIS 3.09 builds this installer; NSIS license and zlib/LZMA notices are provided in licenses/toolchain/nsis-copyright.txt.
The installer is unsigned. The app does not silently install drivers, services, scheduled tasks, firewall rules or startup items.

The macOS arm64 build uses Apple Clang and system frameworks (CoreAudio, CoreMIDI, Cocoa, AVFoundation and related OS frameworks). No Windows toolchain or runtime is included in the Mac application. The Mac app is ad-hoc signed; its PKG is unsigned and neither artifact is notarized.
