param(
    [string]$Dxc = '',
    [string]$SpirvDxc = '',
    [switch]$Spirv,
    [string]$Filter = ''
)

$ErrorActionPreference = 'Continue'
$Root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$ShaderDir = Join-Path $Root 'Shaders'

if (-not $Dxc) {
    $Candidates = @(
        'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe',
        'C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\dxc.exe'
    )
    $Dxc = $Candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

if (-not $Dxc -or -not (Test-Path $Dxc)) {
    throw 'dxc.exe not found. Pass -Dxc <path>.'
}

if ($Spirv) {
    if (-not $SpirvDxc) {
        $SpirvCandidates = @(
            'D:\Tools\DXC\1.9.2609\bin\x64\dxc.exe',
            'D:\vcpkg\installed\x64-windows\bin\dxc.exe',
            'D:\VulkanSDK\1.2.162.0\Bin\dxc.exe'
        )
        $SpirvDxc = $SpirvCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    }
    if (-not (Test-Path $SpirvDxc)) {
        throw 'SPIR-V dxc.exe not found. Pass -SpirvDxc <path>.'
    }
    $Dxc = $SpirvDxc
}

$OutputRoot = Join-Path $PSScriptRoot ('out\' + $(if ($Spirv) { 'spirv' } else { 'dxil' }))
New-Item -ItemType Directory -Force $OutputRoot | Out-Null

$Cases = [System.Collections.Generic.List[object]]::new()

foreach ($Key in 'UINT', 'INT', 'FLOAT') {
    $Cases.Add([pscustomobject]@{
        Name    = "upfront_$Key"
        File    = 'Shaders\ParallelSort\OneSweepUpfrontHist.hlsl'
        Entry   = 'CSUpfrontHist'
        Defines = @("-DONESWEEP_KEY_$Key=1")
    })
}

$Cases.Add([pscustomobject]@{
    Name    = 'exclusive_sum'
    File    = 'Shaders\ParallelSort\OneSweepExclusiveSum.hlsl'
    Entry   = 'CSGlobalExclusiveSum'
    Defines = @()
})

foreach ($Key in 'UINT', 'INT', 'FLOAT') {
    foreach ($Pair in $false, $true) {
        foreach ($Bits in $false, $true) {
            $Defines = @("-DONESWEEP_KEY_$Key=1")
            if ($Bits) {
                $Defines += '-DUSE_16BIT_ONESWEEP=1'
            }
            if ($Pair) {
                foreach ($Payload in 'UINT', 'INT', 'FLOAT') {
                    $Cases.Add([pscustomobject]@{
                        Name    = "digit_${Key}_pair_${Payload}_16_$Bits"
                        File    = 'Shaders\ParallelSort\OneSweepDigitBinning.hlsl'
                        Entry   = 'CSChainedScanDigitBinning'
                        Defines = @($Defines + @('-DUSE_PAIR_ONESWEEP=1', "-DONESWEEP_PAYLOAD_$Payload=1"))
                    })
                }
            }
            else {
                $Cases.Add([pscustomobject]@{
                    Name    = "digit_${Key}_key_only_16_$Bits"
                    File    = 'Shaders\ParallelSort\OneSweepDigitBinning.hlsl'
                    Entry   = 'CSChainedScanDigitBinning'
                    Defines = $Defines
                })
            }
        }
    }
}

foreach ($Is64 in $false, $true) {
    foreach ($Entry in 'CSBitonicPreSort', 'CSBitonicInnerSort') {
        $Cases.Add([pscustomobject]@{
            Name    = "bitonic_${Entry}_64_$Is64"
            File    = 'Shaders\ParallelSort\BitonicSort.hlsl'
            Entry   = $Entry
            Defines = @("-DUSE_64BIT_BITONICSORT=$([int]$Is64)")
        })
    }
    $Cases.Add([pscustomobject]@{
        Name    = "bitonic_CSBitonicOuterSort_64_$Is64"
        File    = 'Shaders\ParallelSort\BitonicSort.hlsl'
        Entry   = 'CSBitonicOuterSort'
        Defines = @("-DUSE_64BIT_BITONICSORT=$([int]$Is64)", '-DUSE_BITONIC_METHOD=2')
    })
}

$Selected = $Cases
if ($Filter) {
    $Selected = @($Cases | Where-Object { $_.Name -like "*$Filter*" })
}

$Passed = 0
$Failed = 0
$Skipped = 0

foreach ($Case in $Selected) {
    $Source = Join-Path $Root $Case.File
    if (-not (Test-Path $Source)) {
        Write-Host "SKIP $($Case.Name): source not found"
        $Skipped++
        continue
    }

    $Output = Join-Path $OutputRoot ($Case.Name + $(if ($Spirv) { '.spv' } else { '.dxil' }))
    $DxcArgs = @('-T', 'cs_6_6', '-E', $Case.Entry, '-HV', '2021', '-enable-16bit-types', '-I', $ShaderDir)
    if ($Spirv) {
        $DxcArgs += @('-spirv', '-fspv-target-env=vulkan1.2')
    }
    else {
        $DxcArgs += '-DUSE_GLOBALLY_CORHERENT=0'
    }
    $DxcArgs += $Case.Defines
    $DxcArgs += @('-Fo', $Output, $Source)

    $Log = & $Dxc @DxcArgs 2>&1 | Out-String
    if ($LASTEXITCODE -eq 0 -and (Test-Path $Output)) {
        Write-Host "PASS $($Case.Name)"
        $Passed++
    }
    else {
        Write-Host "FAIL $($Case.Name)"
        $Log -split "`r?`n" | Where-Object { $_ -match 'error:' } | Select-Object -First 6 | ForEach-Object { Write-Host $_ }
        $Failed++
    }
}

Write-Host "Sort shader compile: total=$($Selected.Count) pass=$Passed fail=$Failed skip=$Skipped"
if ($Failed -gt 0) {
    exit 1
}
