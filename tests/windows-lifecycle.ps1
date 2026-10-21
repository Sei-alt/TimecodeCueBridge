# Run in a clean Windows test account/VM. Does not remove existing user settings.
param([Parameter(Mandatory=$true)][string]$Setup)
$ErrorActionPreference='Stop'
$install=Join-Path $env:LOCALAPPDATA 'Programs/TimecodeCueBridge'
$data=Join-Path $env:APPDATA 'TimecodeCueBridge'
$desktopShortcut=Join-Path ([Environment]::GetFolderPath('DesktopDirectory')) 'Timecode Cue Bridge.lnk'
$key='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\TimecodeCueBridge'
if(Test-Path $install){throw 'Use a clean test account: an existing installation is present'}
if(Test-Path $data){throw 'Use a clean test account: existing user data must be preserved'}
if(Test-Path $desktopShortcut){throw 'Use a clean test account: an existing desktop shortcut must be preserved'}
function Run-Wait([string]$exe,[string]$arguments='') {
  $p=Start-Process -FilePath $exe -ArgumentList $arguments -PassThru
  if(!$p.WaitForExit(30000)){throw "Timed out: $exe"}
  $p.Refresh(); return $p.ExitCode
}
function Check-DesktopShortcut {
  if(!(Test-Path $desktopShortcut)){throw 'Desktop shortcut missing'}
  $shell=New-Object -ComObject WScript.Shell
  try {
    $shortcut=$shell.CreateShortcut($desktopShortcut)
    if($shortcut.TargetPath -ine (Join-Path $install 'TimecodeCueBridge.exe')){throw 'Desktop shortcut target incorrect'}
    $iconPath=($shortcut.IconLocation -replace ',\s*-?\d+\s*$','').Trim('"')
    if($iconPath -ine (Join-Path $install 'TimecodeCueBridge.exe')){throw 'Desktop shortcut icon incorrect'}
  } finally {
    if($null -ne $shortcut){[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shortcut)}
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)
  }
}
if((Run-Wait (Resolve-Path $Setup).Path '/S') -ne 0){throw 'Install failed'}
Check-DesktopShortcut
if(!(Test-Path "$install/TimecodeCueBridge.exe")){throw 'Application missing'}
if(!(Test-Path $key)){throw 'Installed Apps registry missing'}
$reg=Get-ItemProperty $key
if($reg.DisplayName -ne 'Timecode Cue Bridge' -or $reg.UninstallString -ne ('"'+$install+'\Uninstall.exe"')){throw 'Installed Apps registration invalid'}
if(!(Test-Path (Join-Path $env:APPDATA 'Microsoft/Windows/Start Menu/Programs/Timecode Cue Bridge.lnk'))){throw 'Start Menu shortcut missing'}
if((Run-Wait "$install/TimecodeCueBridge.exe" '--smoke-test') -ne 0){throw 'GUI launch/exit failed'}
New-Item -Force -ItemType Directory $data | Out-Null
'{"version":2,"host":"127.0.0.1","port":9000,"fps":30,"cues_text":"00:00:01:00 | test | /test | 1"}' | Set-Content "$data/cues.json" -Encoding utf8
'preserve this log' | Set-Content "$data/events.log"
'unknown user file' | Set-Content "$install/keep-me.txt"
$configHash=(Get-FileHash "$data/cues.json").Hash
$logHash=(Get-FileHash "$data/events.log").Hash
# Mutex guard must refuse uninstall and upgrade while app runs.
$app=Start-Process "$install/TimecodeCueBridge.exe" -PassThru
try {
  Start-Sleep -Seconds 3
  if($app.HasExited){throw 'GUI exited unexpectedly'}
  # _?= prevents NSIS spawning a temp child and makes the exit status observable.
  if((Run-Wait "$install/Uninstall.exe" ('/S _?='+$install)) -ne 3){throw 'Running app uninstall guard failed'}
  if((Run-Wait (Resolve-Path $Setup).Path '/S') -ne 3){throw 'Running app update guard failed'}
} finally {
  if(!$app.HasExited){$null=$app.CloseMainWindow(); if(!$app.WaitForExit(10000)){throw 'GUI did not close cleanly'}}
}
# Re-hash after normal logging from the running-app check.
$configHash=(Get-FileHash "$data/cues.json").Hash
$logHash=(Get-FileHash "$data/events.log").Hash
if((Run-Wait "$install/Uninstall.exe" ('/S _?='+$install)) -ne 0){throw 'Uninstall failed'}
if(Test-Path "$install/TimecodeCueBridge.exe"){throw 'Program was not removed'}
if(Test-Path $key){throw 'Installed Apps registry was not removed'}
if(Test-Path $desktopShortcut){throw 'Desktop shortcut was not removed'}
if((Get-FileHash "$data/cues.json").Hash -ne $configHash){throw 'Cue settings changed during uninstall'}
if((Get-FileHash "$data/events.log").Hash -ne $logHash){throw 'Logs changed during uninstall'}
if(!(Test-Path "$install/keep-me.txt")){throw 'Unknown user file deleted'}
if((Run-Wait (Resolve-Path $Setup).Path '/S') -ne 0){throw 'Reinstall failed'}
Check-DesktopShortcut
if((Get-FileHash "$data/cues.json").Hash -ne $configHash){throw 'Cue settings changed on reinstall'}
if((Run-Wait "$install/TimecodeCueBridge.exe" '--smoke-test') -ne 0){throw 'Reinstalled app launch failed'}
$logHash=(Get-FileHash "$data/events.log").Hash
if((Run-Wait "$install/Uninstall.exe" ('/S _?='+$install)) -ne 0){throw 'Second uninstall failed'}
if(Test-Path $desktopShortcut){throw 'Reinstalled desktop shortcut was not removed'}
if((Get-FileHash "$data/cues.json").Hash -ne $configHash){throw 'Reinstalled config not retained'}
if((Get-FileHash "$data/events.log").Hash -ne $logHash){throw 'Reinstalled log not retained'}
Write-Host 'PASS: install, desktop shortcut target/icon/removal, Installed Apps, GUI launch/exit, running app guard, uninstall, retained data, reinstall'
