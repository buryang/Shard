# Sort Shader Baseline

## Environment

- Windows SDK DXC: 1.8.2502.11.
- SPIR-V DXC: 1.9.2609.5.
- `spirv-val`: Vulkan SDK 1.2.162.
- GPU: GTX 1650 Ti, 4 GB, driver 466.11.

## Compile

```powershell
.\Test\ShaderCompile\CompileSortShaders.ps1
.\Test\ShaderCompile\CompileSortShaders.ps1 -Spirv
```

- DXIL: 34/34 pass.
- SPIR-V: 34/34 pass.
- `spirv-val --target-env vulkan1.2`: 34/34 pass.

## GPU

```powershell
.\Test\ShaderSort\gpu\build.ps1
.\Test\ShaderSort\gpu\RunSortTests.ps1
```

- OneSweep: 9 modes pass. Each mode covers 13 size and direction cases.
- Bitonic: 32 cases pass.

The GPU harness uses D3D12 compute and DXIL. Vulkan/SPIR-V runtime is not covered because the current machine reports no Vulkan ICD.

## Known Fixes

- Use `THREAD_WARP_SIZE` for wave index math.
- The wave<16 path is excluded for NVIDIA and AMD target wave sizes.
- Reverse histogram uses the same flipped key domain as ranking.
- Lookback status mask is `0x3`.
- `WaveHistReductionExclusiveScanWarpGE16` uses a 256-entry shared scan.
