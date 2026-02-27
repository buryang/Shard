#pragma once
#include <thread>
#include "Utils/CommonUtils.h"

#ifdef USE_RAWINPUT
#include <window.h>
#elif defined(USE_SDL)
#include <SDL3/SDL.h>
#endif

void mm_pause()
{
#ifdef _MSC_VER
    _mm_pause();
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_ia32_pause();
#else
    std::this_thread::yield();
#endif
}

constexpr uint64_t count_zero(uint64_t val)
{
#if __cplusplus >= 202002L
    return std::countr_zero(val);
#elif _MSC_VER
    return _tzcnt_u64(val);
#else
    return __builtin_ctzll(val);
#endif
}


namespace Shard::Utils
{
#ifdef USE_RAWINPUT
	using WindowHandle = HWND;
#elif defined(USE_SDL)
	using WindowHandle = SDL_Window*;
#endif
	using PID = size_type;

	//thread
#if __cplusplus < 202002L
	void BindThreadToCPU(std::thread& th, uint32_t core_id);
	uint32_t GetCPUBindToThread(std::thread& th);
    void SetThreadPriority(std::thread& th, int32_t policy, int32_t priority);
#else
	void BindThreadToCPU(std::jthread& th, uint32_t core_id);
	uint32_t GetCPUBindToThread(std::jthread& th);
	void SetThreadPriority(std::jthread& th, int32_t policy, int32_t priority);
#endif 

	//process
	bool IsProcessRunning(PID process_id);
	void TerminateProcess(PID process_id);

	//monitor
	uint32_t GetMonitorRefreshRate();

	struct MonitorSetting
	{
		uint32_t	fps_;
		uint32_t    bits_per_pixel_;
		float2		resolution_;
	};

	const MonitorSetting& GetMonitorSetting();
}
