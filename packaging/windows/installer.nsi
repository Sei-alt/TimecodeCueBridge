Target amd64-unicode
Unicode True
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!ifndef APP_BINARY
  !error "APP_BINARY is required"
!endif
!ifndef OUTPUT_DIR
  !define OUTPUT_DIR "."
!endif
!ifndef SOURCE_DIR
  !define SOURCE_DIR "../.."
!endif
!ifndef VERSION
  !define VERSION "0.5.1"
!endif
!define APP_NAME "Timecode Cue Bridge"
!define APP_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\TimecodeCueBridge"
Icon "${SOURCE_DIR}/assets/app.ico"
UninstallIcon "${SOURCE_DIR}/assets/app.ico"
Name "${APP_NAME}"
OutFile "${OUTPUT_DIR}/TimecodeCueBridge-${VERSION}-Win11-x64-Setup.exe"
RequestExecutionLevel user
SetCompressor /SOLID lzma
InstallDir "$LocalAppData\Programs\TimecodeCueBridge"
InstallDirRegKey HKCU "${APP_KEY}" "InstallLocation"
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${APP_NAME}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "Timecode Cue Bridge Setup (x64)"
VIAddVersionKey "LegalCopyright" "Timecode Cue Bridge Project"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SOURCE_DIR}/LICENSE"
; Fixed per-user program directory avoids installing over arbitrary user folders.
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Japanese"
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP|MB_OK "This application requires Windows x64."
    SetErrorLevel 2
    Abort
  ${EndIf}
  SetRegView 64
  SetShellVarContext current
  ; Command line /D cannot redirect this installer into a data/system directory.
  StrCpy $INSTDIR "$LocalAppData\Programs\TimecodeCueBridge"
FunctionEnd

Function CheckAppClosed
  System::Call 'kernel32::OpenMutexW(i 0x100000, i 0, w "Local\TimecodeCueBridge.Running") p .r0'
  ${If} $0 != 0
    System::Call 'kernel32::CloseHandle(p r0)'
    IfSilent silent_busy
    MessageBox MB_ICONEXCLAMATION|MB_OK "Close Timecode Cue Bridge and try again."
    silent_busy:
    SetErrorLevel 3
    Abort
  ${EndIf}
FunctionEnd

Section "Timecode Cue Bridge" SEC_PROGRAM
  Call CheckAppClosed
  SetOutPath "$INSTDIR"
  ClearErrors
  File /oname=TimecodeCueBridge.exe "${APP_BINARY}"
  IfErrors install_failed
  File /oname=LICENSE.txt "${SOURCE_DIR}/LICENSE"
  File /oname=THIRD_PARTY_NOTICES.txt "${SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
  File /oname=README_JA.txt "${SOURCE_DIR}/README_JA.md"
  File /oname=AGPL-3.0.txt "${SOURCE_DIR}/licenses/AGPL-3.0.txt"
  File /oname=LGPL-3.0.txt "${SOURCE_DIR}/licenses/LGPL-3.0.txt"
  File /oname=GPL-3.0.txt "${SOURCE_DIR}/licenses/GPL-3.0.txt"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortcut "$SMPROGRAMS\Timecode Cue Bridge.lnk" "$INSTDIR\TimecodeCueBridge.exe"
  CreateShortcut "$DESKTOP\Timecode Cue Bridge.lnk" "$INSTDIR\TimecodeCueBridge.exe" "" "$INSTDIR\TimecodeCueBridge.exe" 0
  WriteRegStr HKCU "${APP_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKCU "${APP_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${APP_KEY}" "Publisher" "Timecode Cue Bridge Project"
  WriteRegStr HKCU "${APP_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${APP_KEY}" "DisplayIcon" "$INSTDIR\TimecodeCueBridge.exe,0"
  WriteRegStr HKCU "${APP_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "${APP_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKCU "${APP_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${APP_KEY}" "NoRepair" 1
  WriteRegDWORD HKCU "${APP_KEY}" "EstimatedSize" 18000
  Goto install_done
  install_failed:
    IfSilent +2
    MessageBox MB_ICONSTOP|MB_OK "Cannot write the application. Close it and retry."
    SetErrorLevel 4
    Abort
  install_done:
SectionEnd

Function un.onInit
  SetRegView 64
  SetShellVarContext current
  ; Only permit removal in the designated per-user program directory.
  StrCpy $1 "$LocalAppData\Programs\TimecodeCueBridge"
  ${If} $INSTDIR != $1
    IfSilent +2
    MessageBox MB_ICONSTOP|MB_OK "Unexpected installation path; no files were removed."
    SetErrorLevel 5
    Abort
  ${EndIf}
  System::Call 'kernel32::OpenMutexW(i 0x100000, i 0, w "Local\TimecodeCueBridge.Running") p .r0'
  ${If} $0 != 0
    System::Call 'kernel32::CloseHandle(p r0)'
    IfSilent +2
    MessageBox MB_ICONEXCLAMATION|MB_OK "Close Timecode Cue Bridge before uninstalling."
    SetErrorLevel 3
    Abort
  ${EndIf}
FunctionEnd

Section "Uninstall"
  ClearErrors
  Delete "$INSTDIR\TimecodeCueBridge.exe"
  ${If} ${Errors}
    IfSilent +2
    MessageBox MB_ICONSTOP|MB_OK "Application is in use. Close it and retry."
    SetErrorLevel 4
    Abort
  ${EndIf}
  Delete "$SMPROGRAMS\Timecode Cue Bridge.lnk"
  Delete "$DESKTOP\Timecode Cue Bridge.lnk"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\THIRD_PARTY_NOTICES.txt"
  Delete "$INSTDIR\README_JA.txt"
  Delete "$INSTDIR\AGPL-3.0.txt"
  Delete "$INSTDIR\LGPL-3.0.txt"
  Delete "$INSTDIR\GPL-3.0.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "${APP_KEY}"
  ; NEVER delete AppData\Roaming\TimecodeCueBridge (cues.json/events.log).
  ; NEVER recurse or use wildcards in the install directory.
SectionEnd
