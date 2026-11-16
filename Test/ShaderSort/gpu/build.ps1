$ErrorActionPreference = 'Stop'

$Cmake = 'C:\Program Files\CMake\bin\cmake.exe'
$VcVars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
$BuildDir = Join-Path $PSScriptRoot 'build'

if (-not (Test-Path $Cmake)) {
    throw "cmake.exe not found: $Cmake"
}
if (-not (Test-Path $VcVars)) {
    throw "vcvars64.bat not found: $VcVars"
}

$Command = "`"$VcVars`" >nul && `"$Cmake`" -S `"$PSScriptRoot`" -B `"$BuildDir`" -G `"Visual Studio 17 2022`" -A x64"
$Command += " && `"$Cmake`" --build `"$BuildDir`" --config Release"

cmd.exe /d /c $Command
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Built: $(Join-Path $BuildDir 'Release\GpuSortHarness.exe')"
