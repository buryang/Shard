# GPU Harness Results

## Environment

- GPU: NVIDIA GeForce GTX 1650 Ti, 4 GB.
- Driver: 466.11.
- DXIL compiler: Windows SDK DXC 1.8.2502.11.
- Harness: D3D12 compute.

## Commands

```powershell
.\Test\ShaderSort\gpu\build.ps1
.\Test\ShaderSort\gpu\RunSortTests.ps1
```

## Result

- OneSweep: 9 modes PASS. Each mode covers 13 size and direction cases.
- Bitonic: 32 cases PASS.
- Exit code: 0.

The detailed log is in `out.txt` after a run.
