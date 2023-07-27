#pragma once
#include <thread>

#ifdef USE_RAWINPUT
#include <window.h>
#elif defined(USE_SDL)
#include <SDL3/SDL.h>
#endif

namespace Shard::Utils
{
#ifdef USE_RAWINPUT
	using WindowHandle = HWND;
#elif defined(USE_SDL)
	using WindowHandle = SDL_Window*;
#endif
}

namespace Shard::Utils
{
	void BindThreadToCPU(std::thread& th, uint32_t core_id);
	uint32_t GetCPUBindToThread(std::thread& th);
}