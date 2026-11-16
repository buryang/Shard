param(
    [switch]$SkipGpu
)

$ErrorActionPreference = 'Stop'
$Root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Compile = Join-Path $Root 'Test\ShaderCompile\CompileSortShaders.ps1'
$Val = 'D:\VulkanSDK\1.2.162.0\Bin\spirv-val.exe'

if (-not (Test-Path $Compile)) {
    throw "Compile matrix not found: $Compile"
}

& $Compile
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $Compile -Spirv
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$SpirvDir = Join-Path $Root 'Test\ShaderCompile\out\spirv'
$Modules = Get-ChildItem $SpirvDir -Filter *.spv
foreach ($Module in $Modules) {
    & $Val --target-env vulkan1.2 $Module.FullName
    if ($LASTEXITCODE -ne 0) {
        throw "spirv-val failed: $($Module.Name)"
    }
}
Write-Host "spirv-val: $($Modules.Count)/$($Modules.Count) pass"

& python (Join-Path $PSScriptRoot 'property_test.py')
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not $SkipGpu) {
    $GpuRun = Join-Path $PSScriptRoot 'gpu\RunSortTests.ps1'
    $GpuExe = Join-Path $PSScriptRoot 'gpu\build\Release\GpuSortHarness.exe'
    if ((Test-Path $GpuRun) -and (Test-Path $GpuExe)) {
        & $GpuRun
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
    else {
        Write-Host 'GPU harness not built; run Test\ShaderSort\gpu\build.ps1 to enable GPU tests.'
    }
}

Write-Host 'HLSL sort tests passed.'
