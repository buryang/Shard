param(
    [string]$Dxc = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe'
)

$ErrorActionPreference = 'Continue'
$Root = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$ShaderDir = Join-Path $Root 'Shaders'
$OutDir = Join-Path $PSScriptRoot 'shaders'
$Exe = Join-Path $PSScriptRoot 'build\Release\GpuSortHarness.exe'
$Common = @('-T', 'cs_6_6', '-HV', '2021', '-I', $ShaderDir, '-DUSE_GLOBALLY_CORHERENT=0')

New-Item -ItemType Directory -Force $OutDir | Out-Null

function Compile-OneSweep([string]$Key, [bool]$Pair, [bool]$Bits16) {
    $KeyDefine = "-DONESWEEP_KEY_$($Key.ToUpper())=1"
    $Extra = @($KeyDefine)
    if ($Bits16) {
        $Extra += @('-enable-16bit-types', '-DUSE_16BIT_ONESWEEP=1')
    }
    if ($Pair) {
        $Extra += @('-DUSE_PAIR_ONESWEEP=1', '-DONESWEEP_PAYLOAD_UINT=1')
    }

    & $Dxc @Common @Extra -E CSUpfrontHist -Fo (Join-Path $OutDir 'CSUpfrontHist.cso') (Join-Path $Root 'Shaders\ParallelSort\OneSweepUpfrontHist.hlsl') | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "DXIL compile failed: hist $Key pair=$Pair bits16=$Bits16" }

    & $Dxc @Common @Extra -E CSChainedScanDigitBinning -Fo (Join-Path $OutDir 'CSChainedScanDigitBinning.cso') (Join-Path $Root 'Shaders\ParallelSort\OneSweepDigitBinning.hlsl') | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "DXIL compile failed: digit $Key pair=$Pair bits16=$Bits16" }
}

& $Dxc @Common -E CSGlobalExclusiveSum -Fo (Join-Path $OutDir 'CSGlobalExclusiveSum.cso') (Join-Path $Root 'Shaders\ParallelSort\OneSweepExclusiveSum.hlsl') | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'DXIL compile failed: scan' }

& $Dxc @Common -E CSProbe -Fo (Join-Path $OutDir 'CSProbe.cso') (Join-Path $PSScriptRoot 'probe.hlsl') | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'DXIL compile failed: probe' }

& $Dxc @Common -E CSBitonicPreSort -DUSE_64BIT_BITONICSORT=0 -Fo (Join-Path $OutDir 'CSBitonicPreSort32.cso') (Join-Path $Root 'Shaders\ParallelSort\BitonicSort.hlsl') | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'DXIL compile failed: bitonic32' }

& $Dxc @Common -E CSBitonicPreSort -DUSE_64BIT_BITONICSORT=1 -Fo (Join-Path $OutDir 'CSBitonicPreSort64.cso') (Join-Path $Root 'Shaders\ParallelSort\BitonicSort.hlsl') | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'DXIL compile failed: bitonic64' }

$Cases = @(
    @{ Key = 'uint'; Pair = $false; Bits16 = $false; Mode = 'single' },
    @{ Key = 'uint'; Pair = $true;  Bits16 = $false; Mode = 'pair' },
    @{ Key = 'int';  Pair = $false; Bits16 = $false; Mode = 'single' },
    @{ Key = 'int';  Pair = $true;  Bits16 = $false; Mode = 'pair' },
    @{ Key = 'float'; Pair = $false; Bits16 = $false; Mode = 'single' },
    @{ Key = 'float'; Pair = $true;  Bits16 = $false; Mode = 'pair' },
    @{ Key = 'uint'; Pair = $true;  Bits16 = $true;  Mode = 'pair' },
    @{ Key = 'int';  Pair = $false; Bits16 = $true;  Mode = 'single' },
    @{ Key = 'float'; Pair = $false; Bits16 = $true;  Mode = 'single' }
)

$Passed = 0
foreach ($Case in $Cases) {
    Compile-OneSweep $Case.Key $Case.Pair $Case.Bits16
    Push-Location $PSScriptRoot
    try {
        & $Exe $Case.Key $Case.Mode | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "GPU test failed: $($Case.Key) $($Case.Mode) bits16=$($Case.Bits16)"
        }
    }
    finally {
        Pop-Location
    }
    $Passed++
}

Push-Location $PSScriptRoot
try {
    & $Exe bitonic | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'GPU test failed: bitonic' }
}
finally {
    Pop-Location
}

Write-Host "GPU sort tests passed: OneSweep=$Passed Bitonic=1"
