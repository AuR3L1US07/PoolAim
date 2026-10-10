param([string]$OutputExe = (Join-Path $PSScriptRoot "../../PoolAim.exe"))
$ErrorActionPreference='Stop'
$taskBin=Join-Path $PSScriptRoot '../vendor/toolchain/w64devkit/bin'
Push-Location $PSScriptRoot
try {
 & (Join-Path $taskBin 'windres.exe') resources.rc -o resources.o
 if($LASTEXITCODE -ne 0){throw 'Resource build failed'}
 & (Join-Path $taskBin 'g++.exe') -std=c++17 -O2 -Wall -Wextra -static -D_WIN32_WINNT=0x0601 -municode -mwindows ../src/launcher.cpp resources.o -o $OutputExe -lgdi32 -luser32 -lshell32
 if($LASTEXITCODE -ne 0){throw 'Launcher build failed'}
} finally { Pop-Location }

