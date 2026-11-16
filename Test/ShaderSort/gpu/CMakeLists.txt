cmake_minimum_required(VERSION 3.20)
project(GpuSortHarness LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(GpuSortHarness main.cpp)
target_compile_definitions(GpuSortHarness PRIVATE UNICODE _UNICODE NOMINMAX)
target_link_libraries(GpuSortHarness PRIVATE d3d12 dxgi)
