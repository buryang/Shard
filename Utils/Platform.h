#pragma once
#include <thread>
#include "Utils/CommonUtils.h"

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
	using PID = size_type;
}

namespace Shard::Utils
{
	//thread
	void BindThreadToCPU(std::thread& th, uint32_t core_id);
	uint32_t GetCPUBindToThread(std::thread& th);
	void SetThreadPriority(std::thread& th, int32_t policy, int32_t priority);

	//process
	bool IsProcessRunning(PID process_id);
	void TerminateProcess(PID process_id);

	//monitor
	uint32_t GetMonitorRefreshRate();

	struct MonitorSetting
	{
		uint32_t	fps_;
		uint32_t	bits_per_pixel_;
		vec2	resolution_;
	};

	const MonitorSetting& GetMonitorSetting();
}
