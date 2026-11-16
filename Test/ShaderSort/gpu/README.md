# GPU Sort Harness

This harness runs the sort shaders on the GPU and compares the result with the CPU reference.

It uses D3D12 compute and DXIL. It validates the HLSL algorithm logic. It does not validate the Vulkan SPIR-V runtime path.

## Build

```powershell
.\Test\ShaderSort\gpu\build.ps1
```

## Run

```powershell
.\Test\ShaderSort\gpu\RunSortTests.ps1
```

`RunSortTests.ps1` compiles the required DXIL entries with DXC, then runs the harness.

## Coverage

- OneSweep: uint, int, float keys; key-only and key-payload; 16-bit; ascending and descending; 1 to 5000 elements.
- Bitonic: 32-bit key and 64-bit key-index; ascending and descending; 1 to 2048 elements.

## Limit

The current driver has no Vulkan ICD. Vulkan/SPIR-V runtime coverage requires a working NVIDIA Vulkan driver and a Vulkan harness.
