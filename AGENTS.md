# Shard

Shard is a data-driven C++20 real-time rendering engine for Windows.
Shard has a Vulkan HAL, a render graph, bindless descriptors, hardware ray tracing, meshlets, and an ECS.

- `Graphics/HAL` is the hardware abstraction layer. The Vulkan backend is in `Graphics/HAL/Vulkan`. `/API` wraps native Vulkan.
  The HAL owns the descriptor heaps, the timeline synchronization, the memory residency, and the resource pool.
- `Graphics/Render` holds the render graph (`RenderGraph`, `RenderGraphBuilder`, `RenderGraphExe`), the render passes, and the barriers.
  `Graphics/Render` also holds the shader factory and archive, and the DXC shader compiler.
- `Graphics/Core` holds the shared graphics definitions and the engine-global parameters.
- `Utils`, `Scene`, `System`, `Runtime`, `Example`, `Shaders`, and `Test` hold the utilities, the scene model, and the engine systems.
  These directories also hold the runtime core, the samples, the HLSL shaders, and the GoogleTest tests.
- Build the engine with `ShardEngine.sln` (MSVC, C++20, `v145` toolset). The projects are `Shard`, `Example`, and `UnitTest`.
  The test sources are in `Test/`.
- Shader settings are in `shadertoolsconfig.json`. See `Shaders/AGENTS.md` for HLSL conventions.
- Include paths start at the repository root and `Graphics/`.

# Coding Guidelines

- Write simple, efficient, minimal C++ code.
- Target one fixed feature set on the latest GPUs and drivers.
  Do not use optional feature flags, feature queries, or fallback paths.
  Preserve the supported baseline and the documented API capability exceptions.
  Ask the user before you add a feature that the target hardware does not support universally.
  Do not emulate unsupported operations silently.
- Do not add obvious comments, such as "arguments must be live objects".
  C++ programmers know that the use of a destroyed object is invalid.
- Do not add asserts or comments about 32-bit overflow cases. The existing 32-bit ranges are sufficient.
- Use the engine type vocabulary. Do not use ad-hoc standard-library types.
  Prefer the aliases in `Utils/CommonUtils.h`:
  `Vector`, `SmallVector`, `Array`, `Map`, `HashMap`, `Set`, `Queue`, `Stack`, `List`, `BitSet`, `BitVector`, `Span`, `Optional`, `String`, `Tuple`,
  `float2/3/4`, `float3x3/4x4`, `int2/3/4`, `uint2/3/4`, `quat`.
- Use fixed-width integer types (`uint8_t`, `uint32_t`, `uint64_t`, `int32_t`, ...) and `size_type` consistently.
- Do not add a named variable for a trivial expression or a value that the code uses one time.
- Do not use `auto` for local variables, ordinary value types, or structure types.
- Use exceptions only at initialization boundaries, for invalid input, and for unimplemented paths.
  Do not throw from hot paths. Do not use exceptions for control flow.
- Perform error checks as early as possible.
  Check application initialization, asset loading, and HAL object creation immediately.
  Normal code must not fail after initialization.
- Treat the HAL as a low-level thin wrapper, not as a validation layer.
  Validate only the inputs and the state whose misuse can crash the wrapper.
  Document these preconditions. Enforce programmer errors with asserts.
- The user is responsible for the data and the parameters that the user passes to Vulkan.
  Do not duplicate Vulkan validation for the data.
  Enable the Vulkan validation layer during development.
- Do not destroy a HAL resource while a GPU frame uses the resource.
  Destroy the resource after the submission timeline for the final use completes.
  You can also let the render graph and the HAL resource pool retire the resource.
- At shutdown, wait for the device to idle (`VulkanDevice::WaitIdle`).
  Then drain the render graph and the resource pools.
  Then destroy the resources and the backend device.
- Wait for every submitted frame to drain before you destroy the device.
- Group APIs logically in public headers. Keep all command buffer APIs together.
- Keep each resource creation function next to its destruction function.
  Keep the shared lifetime policy in one place, not per resource type.
- Do not abort for programming errors. Enforce the documented preconditions with asserts.
  Keep release builds free of these checks.
- Use error codes and error messages only for invalid external input and initialization failures.
- Do not check for memory allocation failures. The user must manage memory usage correctly.
- Do not write gratuitous lambdas or complex templates. Keep lambdas short and templates shallow.
- Do not write trivial single-line wrapper functions or single-element wrapper structs.
- Do not allocate memory in hot paths. This rule includes short-lived local vectors.
- Prefer direct value members and references.
  Use `shared_ptr` or `unique_ptr` only for genuinely shared ownership or a custom deleter.
  Use `Handle` and `ResourceManager` for identity-preserving resources.
- Prefer composition over inheritance.
  Virtual interfaces are reserved for the HAL and backend seams (`HALResource`, `HALCommandContext`, `DirectedAcyclicGraph`).
  Do not add new virtual hierarchies for convenience.
- Do not use PIMPL interfaces. The HAL provides the abstraction seam.
- Prefer straightforward loops in performance-critical paths.
  Do not route hot iterations through `<algorithm>`.
- `HashMap` (robin_hood) is the default hash map. Use `Map` and `Set` for the EASTL variants.
- Queues and command pools are externally synchronized.
  Independent pools, queues, resource creation, and timeline waits can run concurrently.
  Ordinary work must not use internal mutexes.
  Restrict synchronization to shared bookkeeping and native API requirements.
  Use atomics where appropriate. Keep diagnostic workarounds out of production paths.
  Document backend-specific exceptions with the implementation.
- Do not copy large user data structures.
  Prefer references to structures. Use spans for array data in structures and parameters.
- Pass array data as `Span`.
  Always pass `Span` parameters by value, so the compiler passes the pointer and the size in registers.
  Review all code against this rule after every change.
- Use C++20 designated initializers with named fields for structures.
  Give public API structure fields useful default values.
  Initialize only the non-default fields and name every initialized field.
- Use PascalCase for namespaces, types, functions, and methods.
  Use the `E` prefix for enum types and the `e` prefix for enumerators.
  Use snake_case for variables, parameters, and members.
  Add a trailing `_` to member names. Use SCREAMING_SNAKE for macros.
- Put a primary type in a header and source pair named after the type.
  Put template and inline implementations in a matching `.inl` file.
- Use `#pragma once` in headers.
  Use `MINIT_API` on exported engine symbols.
  Use `DISALLOW_COPY_AND_ASSIGN` for non-copyable types.
- Use `glog` (`LOG`, `CHECK`) for logging and `fmt` for formatting.
- Vertex shaders, pixel shaders, and shader variants share one shader source file.
  Put unrelated shaders in separate files. Do not use preprocessor conditionals for unrelated shaders.
- Keep the CPU-shared declarations of a shader in its matching shared header.
  Keep shader-specific root data and constants in that header.
  Put types used by multiple shader files in a neutral common header.
- Use shared CPU/GPU data headers for structs, enums, and constants.
  Guard the C++ side with `#ifdef __cplusplus`.
  Do not put arbitrary C/C++ code in the shader-side branch.
- Always review code for performance issues before you consider the work complete.
- Keep the line length at 160 characters. Do not split expressions when the split is not necessary.

# Documentation Guidelines

- Keep contributor rules generic.
  Put implementation details, limits, build commands, and platform exceptions in the relevant documentation.
- Describe the current design and the remaining limitations. Remove obsolete workarounds.
- Keep Markdown documentation short and precise.
  Focus on user-facing behavior and the Shard architecture.
  Emphasize the Vulkan HAL, the bindless descriptors, the render graph, and the ECS.
- Do not include obvious C/C++ conventions, internal plumbing, or exhaustive API catalogs.
  Keep sample-specific details in the source files.
- Keep example descriptions brief.

# Commit Message Guidelines

- Use Conventional Commits 1.0.0.
- Use the format `<type>(<scope>): <description>`.
- Write the subject in the imperative mood.
- Keep the subject lowercase and shorter than 50 characters.
- Do not end the subject with a period.
- Wrap the body at 72 characters.
- Put one blank line between the subject and the body.
- Put one blank line between the body and the footer.
- Use `BREAKING CHANGE:` in the footer or append `!` after the type for breaking changes.
- Keep one logical change in one commit.
- Use these types:
  - `feat`: add a feature.
  - `fix`: fix a bug.
  - `docs`: change documentation.
  - `style`: change formatting without logic changes.
  - `refactor`: restructure code without behavior changes.
  - `perf`: improve performance.
  - `test`: add or change tests.
  - `build`: change the build system or dependencies.
  - `ci`: change continuous integration.
  - `chore`: make other maintenance changes.
  - `revert`: revert a previous commit.
- Reference an issue with `Fixes #123` or `Refs #123` in the footer.
- Examples:
  - `feat(hlsl): add hlsl-dev skill and sort test harness`
  - `fix(render): correct lookback status mask`
