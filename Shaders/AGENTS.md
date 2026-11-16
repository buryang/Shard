# Shader (HLSL) Guidelines

These rules apply to everything under `Shaders/`.
See the repository root `AGENTS.md` for project-wide rules and C++ rules.

- Use the `hlsl-dev` skill for every HLSL compile, SPIR-V validation, sort shader test, and Nsight debug session.
  The skill lists the compiler paths, the debugger paths, and the compile commands.
- Treat shader work as complete only after the affected entries compile to DXIL and to SPIR-V.
  Validate the SPIR-V module with `spirv-val`.
- Write HLSL 2021 for Shader Model 6.6.
  DXC compiles the shaders at runtime.
  DXC uses a per-shader target profile (`vs_6_6`/`ps_6_6`/`cs_6_6`, and higher).
  The tooling defaults are in `shadertoolsconfig.json`.
- The Vulkan runtime consumes SPIR-V.
  Every shared header and every Vulkan shader entry must compile with DXC `-spirv`.
  Validate the DXIL output and the SPIR-V output separately.
- Use only HLSL features that the DXC SPIR-V backend supports.
  Do not add a feature only because it compiles to DXIL.
  Do not add a fallback path that hides a missing SPIR-V feature.
- Use `.hlsl` for technique source files and pass source files.
  Use `.hlsli` for shared declarations.
  Put unrelated shaders in separate `.hlsl` files.
  Do not use preprocessor conditionals for unrelated shaders.
- Use `#ifndef _NAME_INC_` include guards in `.hlsli` files. Do not use `#pragma once`.
- Keep the CPU-shared structs, enums, and constants of a shader in its matching `.hlsli` header.
  Put types used by multiple shaders in a neutral common header (`CommonUtils.hlsli`, `Platform.hlsli`, `BindlessResources.hlsli`).
- Guard the C++ side of shared headers with `#ifdef __cplusplus`, because the CPU build includes these headers.
  Put shader-side helpers in the `#else` branch.
  Do not put arbitrary C/C++ code in the `#else` branch.
- Bind resources through the bindless arrays in `BindlessResources.hlsli`.
  Index the resources with GPU handles.
  Do not add per-pass descriptor sets for resource types that already have a bindless array.
  The macros `SHARD_BINDLESS_TEXTURE_SPACE` (0), `SHARD_BINDLESS_BUFFER_SPACE` (1), and `SHARD_BINDLESS_SAMPLER_SPACE` (2) fix the descriptor spaces.
  Dedicated spaces start at `SHARD_DEDICATE_SPACE_BEGIN` (10).
  Use the `TEXTURE_SPACE`/`BUFFER_SPACE`/`SAMPLER_SPACE` macros in `register(t#/b#/u#/s#, ...)`.
- Compute entries use `[numthreads(...)]`.
  Keep thread group size constants in shared headers.
  Derive the tile and thread relationships from the constants.
- Encapsulate reusable logic as functions.
  Call the functions from the entry point, so shaders can move to work graphs later.
- Work graph node syntax requires Shader Model 6.8 or later.
  DXC 1.9.2609 can generate DXIL and experimental SPIR-V for work graph nodes.
  The Vulkan device must expose `VK_AMDX_shader_enqueue`.
  Keep `[Shader("node")]`, `[NodeLaunch]`, `[NodeDispatchGrid]`, `NodeOutput`, and `NodeIsProgramEntry` out of shared headers.
  Put work graph entry points in separate `.hlsl` files.
  Compile both targets only when the target device supports the required API.
- Expose per-shader tuning knobs with `#ifndef NAME / #define NAME` defaults at the top of the file.
- Use the `half`/`half2/3/4` macros for fp16. These macros map to `min16float` in HLSL.
- The runtime compiler compiles a named entry point. Keep entry function names stable.
