# GPU Harness Status

## Passed

- OneSweep: 9 modes pass. Each mode covers 13 size and direction cases. This includes uint, int, and float keys; key-only and pair modes; 16-bit; ascending and descending; 1 to 5000 elements.
- Bitonic: 32 cases pass. This includes 32-bit and 64-bit elements; ascending and descending; 1, 31, 32, 33, 100, 1024, 1792, and 2048 elements.

## Limit

- The harness uses D3D12 compute and DXIL.
- The current machine reports no Vulkan ICD.
- Vulkan/SPIR-V runtime is not tested.

## Next Step

- Install a working NVIDIA Vulkan driver.
- Run `vulkaninfo --summary`.
- Port the harness to Vulkan or run the engine compute path.
