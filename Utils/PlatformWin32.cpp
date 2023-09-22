#ifdef _WIN32
#include "Utils/Platform.h"
#include <Windows.h>

namespace Shard::Utils {

	void BindThreadToCPU(std::thread& th, uint32_t core_id) {
		const auto num_cores = std::thread::hardware_concurrency();
		const auto mask = 1u << (core_id % num_cores);
		SetThreadAffinityMask(th.native_handle(), mask);
	}

	uint32_t GetCPUBindToThread(std::thread& th) {
		//no such function
		//return GetThreadAffinityMask(th.bative_handle()); 
		auto old_mask = SetThreadAffinityMask(th.native_handle, 0x1);
		SetThreadAffinityMask(th.native_handle, old_mask);
		return old_mask;
	}

	void SetThreadPriority(std::thread& th, int32_t policy, int32_t priority) {
		SetThreadPriority(th.native_handle(), priority);
	}

	bool IsProcessRunning(PID process_id) {
		uint32_t exit_code{ 0u };
		if (GetExitCodeProcess((HANDLE)process_id), &exit_code)))
		{
			return exit_code == STILL_ACTIVE;
		}
		return true;
	}

	void TerminateProcess(PID process_id) {
		TerminateProcess((Handle)process_id, 0);
		CloseHandle((Handle)process_id);
	}

	uint32_t GetMonitorRefreshRate()
	{
		DEVMODE dm;
		dm.dmSize = sizeof(DEVMODE);
		EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTING, &dm);
		return dm.dmDisplayFrequency();
	}

	const MonitorSetting& GetMonitorSetting()
	{
		static MonitorSetting setting;
		static const auto is_init = [&]()->bool{
			DEVMODE dm;
			dm.dmSize = sizeof(DEVMODE);
			EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTING, &dm);
			setting.fps_ = dm.dmDisplayFrequency;
			setting.bits_per_pixel_ = dm.dmBitsPerPel;
			setting.resolution_ = { dm.dmPelsWidth, dm.dmPelsHeight };
			return true;
		}();
		return setting;
	}
}
#endif