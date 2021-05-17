#pragma once

#ifdef WIN32
#define WINAPI __declspec(dllexport)
#else 
#define WINAPI
#endif 

#include "glm/glm.hpp"
#include <memory>
#include <vector>
#include <set>
#include <array>
#include <algorithm>


class HostMemoryManager
{
public:
	static HostMemoryManager& Create();
	HostMemoryManager(const HostMemoryManager&) = delete;
	HostMemoryManager& operator=(const HostMemoryManager&) = delete;
	void* Alloc();
	void* ReAlloc();
	void Free();
private:
	HostMemoryManager() = default;
};
