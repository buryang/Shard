# Shader (HLSL) Guidelines

These rules apply to everything under `Shaders/`. See the repository root `AGENTS.md` for project-wide and C++ rules.

- Write HLSL 2021 targeting Shader Model 6.6. Shaders are compiled with DXC at runtime (`Graphics/Render/ShaderCompile/RenderDXCCompile.cpp`) using a
  per-shader target profile (`vs_6_6`/`ps_6_6`/`cs_6_6`, and higher). Tooling defaults are in `shadertoolsconfig.json` (include directory `./Shaders`).
- Use `.hlsl` for technique/pass source files and `.hlsli` for shared declarations. Put entirely different shaders in separate `.hlsl` files rather than
  behind preprocessor conditionals.
- Use `#ifndef _NAME_INC_` include guards in `.hlsli` files, not `#pragma once`.
- Keep a shader's CPU-shared structs, enums, and constants in its matching `.hlsli` header; put types used by multiple shaders in a neutral common header
  (`CommonUtils.hlsli`, `Platform.hlsli`, `BindlessResources.hlsli`).
- Guard the C++ side of shared headers with `#ifdef __cplusplus`, because the CPU build includes these headers. Put shader-side helpers in the `#else` branch;
  do not put arbitrary C/C++ code in it.
- Bind resources through the bindless arrays in `BindlessResources.hlsli` and index them with GPU handles. Do not add per-pass descriptor sets for resource
  types that already have a bindless array. Descriptor spaces are fixed by `SHARD_BINDLESS_TEXTURE_SPACE` (0), `SHARD_BINDLESS_BUFFER_SPACE` (1),
  `SHARD_BINDLESS_SAMPLER_SPACE` (2), with dedicated spaces starting at `SHARD_DEDICATE_SPACE_BEGIN` (10); use the `TEXTURE_SPACE`/`BUFFER_SPACE`/`SAMPLER_SPACE`
  macros in `register(t#/b#/u#/s#, ...)`.
- Compute entries use `[numthreads(...)]`; keep thread group size constants in shared headers and derive tile/thread relationships from them.
- Encapsulate reusable logic as functions and call them from the entry point, so shaders can move to work graphs later.
- Expose per-shader tuning knobs with `#ifndef NAME / #define NAME` defaults at the top of the file (for example `BINNING_TECHNIQUE`, `SHADING_VARIABLE_RATE`).
- Use the `half`/`half2/3/4` macros for fp16; they map to `min16float` in HLSL (see `CommonUtils.hlsli`).
- The runtime compiler compiles a named entry point, so keep entry function names stable.
