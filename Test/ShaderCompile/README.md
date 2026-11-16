# Sort Shader Compile

Run the sort shader compile matrix.

```powershell
.\Test\ShaderCompile\CompileSortShaders.ps1
.\Test\ShaderCompile\CompileSortShaders.ps1 -Spirv
```

The script compiles 34 cases with DXC:

- `OneSweepUpfrontHist.hlsl`
- `OneSweepExclusiveSum.hlsl`
- `OneSweepDigitBinning.hlsl`
- `BitonicSort.hlsl`

The matrix covers uint, int, and float keys.
The matrix covers key-only and key-payload modes.
The matrix covers 16-bit and 32-bit local data.
The matrix covers 32-bit and 64-bit bitonic elements.
The matrix covers bitonic pre-sort, inner sort, and outer sort.

Use `-Filter <text>` to select cases.
Use `-Spirv` for SPIR-V output.
The script uses `D:\Tools\DXC\1.9.2609\bin\x64\dxc.exe` for SPIR-V by default.
Use DXC 1.9 or later. DXC 1.7 crashes on `OneSweepDigitBinning.hlsl` with an internal compiler error.
Pass `-SpirvDxc <path>` to use another SPIR-V DXC.
Pass `-Dxc <path>` to override the compiler.

Outputs go to `Test/ShaderCompile/out/dxil` or `Test/ShaderCompile/out/spirv`.
