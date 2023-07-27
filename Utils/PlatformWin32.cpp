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
}