param([string]$CompilerBin = "$PSScriptRoot\vendor\toolchain\w64devkit\bin", [string]$OutputDirectory = "$PSScriptRoot\dist")
$ErrorActionPreference='Stop'
$gcc=Join-Path $CompilerBin 'gcc.exe'
$gxx=Join-Path $CompilerBin 'g++.exe'
if(-not(Test-Path -LiteralPath $gxx)){throw 'x86 w64devkit compiler not found.'}
$taskBuild=Join-Path $PSScriptRoot 'build'
$taskDist=$OutputDirectory
New-Item -ItemType Directory -Path $taskBuild,$taskDist -Force | Out-Null
foreach($source in @('buffer.c','hook.c','trampoline.c','hde/hde32.c')){
 $object=Join-Path $taskBuild ((Split-Path $source -Leaf)+'.o')
 & $gcc -O2 -D_WIN32_WINNT=0x0601 -c (Join-Path $PSScriptRoot ('vendor/minhook/src/'+$source)) -o $object
 if($LASTEXITCODE -ne 0){throw 'MinHook compilation failed.'}
}
$objects=@(Get-ChildItem -LiteralPath $taskBuild -Filter '*.c.o' | ForEach-Object FullName)
& $gxx -std=c++17 -O2 -Wall -Wextra -static -static-libgcc -static-libstdc++ -D_WIN32_WINNT=0x0601 -shared (Join-Path $PSScriptRoot 'src/plugin.cpp') @objects -o (Join-Path $taskDist 'PoolAim.dll') -ld3d9 -lopengl32 -lgdi32 -luser32 -lkernel32
if($LASTEXITCODE -ne 0){throw 'Plugin compilation failed.'}
& $gxx -std=c++17 -O2 -Wall -Wextra -static -municode -D_WIN32_WINNT=0x0601 (Join-Path $PSScriptRoot 'src/injector.cpp') -o (Join-Path $taskDist 'LoadPoolAim.exe') -luser32 -lkernel32
if($LASTEXITCODE -ne 0){throw 'Loader compilation failed.'}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'PoolAim.ini') -Destination (Join-Path $taskDist 'PoolAim.ini') -Force
& $gxx -std=c++17 -O2 -static (Join-Path $PSScriptRoot 'tests/geometry_test.cpp') -o (Join-Path $taskBuild 'geometry_test.exe')
if($LASTEXITCODE -ne 0){throw 'Geometry test compilation failed.'}
& (Join-Path $taskBuild 'geometry_test.exe')
if($LASTEXITCODE -ne 0){throw 'Geometry checks failed.'}
Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskDist 'PoolAim.dll'),(Join-Path $taskDist 'LoadPoolAim.exe') | Format-Table
