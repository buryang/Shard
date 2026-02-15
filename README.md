# Shard: A Data-Driven C++ Rendering Engine
============================================

Shard is a modern, data-driven C++ rendering engine designed for high-performance real-time graphics. It embraces a data-oriented design philosophy to maximize efficiency and scalability on contemporary hardware.


## Project Goals
----------------

1. **Data-Driven C++ Game Engine**: Build a game engine architecture where behavior and rendering are primarily controlled by data, enabling rapid iteration and flexibility.

2. **Entity-Component-System (ECS) Infrastructure**: Implement a high-performance ECS data structure that separates data from logic, optimizing cache utilization and parallel processing.

3. **Render Graph-Based Pipeline**: Design a render pipeline using a render graph abstraction, allowing declarative definition of rendering passes, resource dependencies, and automatic resource management.

4. **Vulkan-Based Hardware Abstraction Layer (HAL)**: Develop a low-level backend leveraging Vulkan for cross-platform support, explicit GPU control, and modern graphics features.

5. **Meshlet-Based LOD & Rasterization**: Implement a Level of Detail (LOD) system using meshlets (grouped GPU-friendly geometry) and corresponding rasterization techniques for efficient rendering of complex scenes.


## Key Features
---------------

- **Data-Driven Architecture**: Engine behavior defined through data, reducing code changes for new features.

- **High-Performance ECS**: Cache-friendly entity-component storage and parallel system execution.

- **Declarative Render Graphs**: Graph-based pipeline definition with automatic resource transitions and barrier insertion.

- **Modern Vulkan Backend**: Low-overhead graphics API utilization with support for latest GPU features.

- **Advanced Geometry Processing**: Meshlet-based rendering for efficient geometry LOD and culling.


## License
----------

This project is licensed under the terms of the [LICENSE](LICENSE) file in the project root directory.

