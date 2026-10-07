# Shard

Data-driven C++20 real-time rendering engine for Windows: a Vulkan HAL, a render-graph pipeline, bindless descriptors, hardware ray tracing, meshlets, and a hybrid ECS.

- `Graphics/HAL`: hardware abstraction layer; the Vulkan backend lives under `Graphics/HAL/Vulkan` (`/API` wraps native Vulkan). Owns bindless descriptor heaps, timeline synchronization, memory residency, and the resource pool.
- `Graphics/Render`: render graph (`RenderGraph`, `RenderGraphBuilder`, `RenderGraphExe`), render passes, barriers, the shader factory/archive, and the runtime DXC shader compiler.
- `Graphics/Core`: shared graphics definitions (`GFXDefinitions.h`) and engine-global parameters.
- `Utils`, `Scene`, `System`, `Runtime`, `Example`, `Shaders`, `Test`: utilities (handles, containers, jobs, ECS), scene model, engine systems, runtime core, samples, HLSL shaders, and GoogleTest tests.
- Build with `ShardEngine.sln` (MSVC, C++20, `v145` toolset). Projects: `Shard` (`EngineCore.vcxproj`), `Example`, `UnitTest`; test sources live in `Test/` and run from the `UnitTest` project.
- Shader settings live in `shadertoolsconfig.json` (HLSL 2021, include directory `./Shaders`). See `Shaders/AGENTS.md` for HLSL conventions.
- Include paths are rooted at the repository root and `Graphics/`: `"Utils/Handle.h"`, `"HAL/HALResources.h"`, `"Render/RenderGraph.h"`, `"Core/GFXDefinitions.h"`.

# Coding Guidelines

- Write simple, efficient, minimal C++ code.
- Target one fixed feature set on the latest GPUs and drivers. Avoid optional feature flags, feature queries, and fallback paths.
  Require features universally supported across the target hardware; otherwise ask the user before adding them.
  Preserve the supported baseline and any documented API capability exceptions. Do not silently emulate unsupported operations.
- Do not add obvious comments such as "arguments must be live objects". C++ programmers already understand that using destroyed objects is invalid.
- Do not add asserts or comments describing 32-bit overflow cases. The existing 32-bit ranges (about 4 billion elements or 4 GB) are sufficient.
- Use the engine's type vocabulary instead of ad-hoc standard-library types. Prefer the aliases in `Utils/CommonUtils.h`:
  `Vector`, `SmallVector`, `Array`, `Map`, `HashMap`, `Set`, `Queue`, `Stack`, `List`, `BitSet`, `BitVector`, `Span`, `Optional`, `String`, `Tuple`,
  and the math aliases `float2/3/4`, `float3x3/4x4`, `int2/3/4`, `uint2/3/4`, `quat`.
- Use fixed-width integer types (`uint8_t`, `uint32_t`, `uint64_t`, `int32_t`, ...) and `size_type` consistently.
- Avoid adding named variables for trivial expressions, especially when the value is used only once.
- Avoid `auto` for local variables. Do not use it for ordinary value or structure types.
- Use exceptions only at initialization/loading boundaries for invalid input or unimplemented paths; do not throw from hot paths or use exceptions for control flow.
- Perform error checks as early as possible. Check application initialization, asset loading, and HAL/Vulkan object creation immediately. Avoid error checking after initialization; normal code should not fail.
- Treat the HAL as a low-level, thin wrapper, not as a validation layer. Validate only inputs and state whose misuse could make the wrapper itself crash.
  Document those preconditions and enforce programmer errors with asserts.
- Data and parameters passed directly from the user to Vulkan are the user's responsibility. Do not duplicate Vulkan validation or maintain shadow state
  solely to validate them; users should enable the Vulkan validation layer during development.
- HAL resource destruction is not safe while recorded or executing GPU frames use the resource. Destroy resources only after the submission timeline
  covering the final use has completed, or let the render graph and HAL resource pool retire them.
- At shutdown, wait for the device to idle (`VulkanDevice::WaitIdle`), drain the render graph and resource pools, then destroy resources and the backend device.
- Wait for every submitted frame to drain before destroying the device.
- Group APIs logically in public headers; for example, keep all command buffer APIs together.
- Keep each public resource creation function next to its matching destruction function. Keep the shared lifetime policy in one place rather than repeating it
  for individual resource types.
- Do not abort for programming errors. Enforce their documented preconditions with asserts and leave release builds free of those checks.
- Use error codes and error messages only for invalid external input data and initialization failures.
- Do not check for memory allocation failures. We cannot recover from running out of memory; managing memory usage properly is the user's responsibility.
- Avoid gratuitous lambdas and complex templates. Callback-style APIs such as the render graph's `AddPass` take lambdas; keep them short and keep templates shallow.
- Avoid trivial single-line wrapper functions.
- Avoid trivial single-element wrapper structs.
- Avoid memory allocations, including short-lived local vectors, in hot paths.
- Prefer direct value members and references. Use `shared_ptr`/`unique_ptr` only where ownership is genuinely shared or needs a custom deleter, and use the
  engine's `Handle`/`ResourceManager` for identity-preserving resources.
- Prefer composition over inheritance. Virtual interfaces are reserved for the HAL/backend seams (`HALResource`, `HALCommandContext`, `DirectedAcyclicGraph`);
  do not add new virtual hierarchies for convenience.
- Do not use PIMPL interfaces; the HAL already provides the abstraction seam.
- Prefer straightforward loops in performance-critical paths; do not route hot iterations through `<algorithm>`.
- Use the engine's hash containers: `HashMap` (robin_hood) is the default hash map, with `Map`/`Set` for the EASTL variants.
- Queues and command pools are externally synchronized; independent pools, queues, resource creation, and timeline waits can run concurrently.
  Keep ordinary recording, lookup and submission free of internal mutexes. Restrict synchronization to shared bookkeeping and native API requirements;
  use atomics where appropriate, and keep diagnostic workarounds isolated from production paths. Document backend-specific exceptions with the implementation.
- Avoid copying large user data structures. Prefer references to structures, and use spans for array data in structures and function parameters.
- Pass array data as `Span` (the engine alias over `eastl::span`). Always pass `Span` function parameters by value; the compiler can pass their pointer-and-size
  fields in registers instead of forcing a memory store/load round trip. Review all code against this rule after every change.
- Use C++20 designated initializers with named fields for structures.
- Give public API structure fields useful default values. At call sites, initialize only the non-default fields and name every initialized field.
- Naming: PascalCase namespaces, types, functions, and methods; `E`-prefixed enum types with `e`-prefixed enumerators; snake_case variables, parameters,
  and members with a trailing `_` on members; SCREAMING_SNAKE macros.
- Put a primary type in a header/source pair named after it. Template and inline implementations go in a matching `.inl` (for example `Utils/Algorithm.h` + `Utils/Algorithm.inl`).
- Use `#pragma once`, `MINIT_API` on exported engine symbols, and `DISALLOW_COPY_AND_ASSIGN` for non-copyable types.
- Use `glog` (`LOG`, `CHECK`) for logging and `fmt` for formatting.
- Vertex and pixel shaders and their variants share a shader source file. Put entirely different shaders in separate files. Do not combine unrelated shaders
  behind preprocessor conditionals.
- Keep each shader source file's CPU-shared declarations in its own matching shared header. Keep shader-specific root data and constants in that header;
  put types used by multiple shader files in a neutral common header.
- Use shared (CPU/GPU) data headers for structs, enums, and constants. Guard the C++ side with `#ifdef __cplusplus`; do not put arbitrary C/C++ code in them
  behind the shader-side branch.
- Always review code for performance issues before considering work complete.
- Line length is 160 characters. Please don't chop expressions to multiple lines if not needed.

# Documentation Guidelines

- Keep contributor rules generic. Put implementation details, limits, build commands and platform exceptions in their relevant documentation.
- Describe the current design and remaining limitations. Remove obsolete workarounds and narratives about fixed intermediate versions.
- Keep Markdown documentation short, precise, and focused on user-facing behavior and fidelity to the Shard architecture. Emphasize the Vulkan HAL, bindless
  descriptors, the render graph, and the ECS. Avoid obvious C/C++ conventions, internal plumbing, exhaustive `Desc` or API catalogs, and sample-specific
  asset or format details better left in source files. Keep example descriptions brief.
