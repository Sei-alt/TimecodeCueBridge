param([string]$BuildDir='build-win', [string]$OutputDir='dist')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $root
try {
  cmake -S . -B $BuildDir -G 'Visual Studio 17 2022' -A x64
  if($LASTEXITCODE -ne 0){throw 'CMake configure failed'}
  cmake --build $BuildDir --config Release --parallel 4
  if($LASTEXITCODE -ne 0){throw 'Build failed'}
  ctest --test-dir $BuildDir -C Release --output-on-failure
  if($LASTEXITCODE -ne 0){throw 'Tests failed'}
  New-Item -ItemType Directory -Force $OutputDir | Out-Null
  $app=(Resolve-Path "$BuildDir/TimecodeCueBridge_artefacts/Release/Timecode Cue Bridge.exe").Path
  $nsis=Get-Command makensis.exe -ErrorAction SilentlyContinue
  if($nsis){$makensis=$nsis.Source}else{$makensis=Join-Path ${env:ProgramFiles(x86)} 'NSIS/makensis.exe'}
  $versionMatch=Select-String -Path (Join-Path $BuildDir 'CMakeCache.txt') -Pattern '^CMAKE_PROJECT_VERSION:STATIC=(\d+\.\d+\.\d+)$'
  if(!$versionMatch){throw 'Cannot read project version from CMakeCache.txt'}
  $version=$versionMatch.Matches[0].Groups[1].Value
  & $makensis /WX "/DAPP_BINARY=$app" "/DOUTPUT_DIR=$((Resolve-Path $OutputDir).Path)" "/DSOURCE_DIR=$root" "/DVERSION=$version" 'packaging/windows/installer.nsi'
  if($LASTEXITCODE -ne 0){throw 'NSIS packaging failed'}
} finally { Pop-Location }
