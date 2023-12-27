#include "Utils/Timer.h"

namespace Shard::Utils {
	
	void Profiler::GetAllProfilers(Map<String, double>& profilers)
	{
		std::shared_lock<std::shared_mutex> rlock(regist_mutex_);
		profilers.insert(record_profilers_.begin(), record_profilers_.end());
	}

	bool Profiler::RegistProfile(const String& name, double elapsed)
	{
		{
			std::shared_lock<std::shared_mutex> rlock(regist_mutex_);
			if (auto iter = record_profilers_.find(name); iter != record_profilers_.end()) {
				iter->second.fetch_add(elapsed, std::memory_order_relaxed);
				return true;
			}
		}
		
		{
			std::unique_lock<std::shared_mutex> wlock(regist_mutex_);
			record_profilers_.insert(eastl::make_pair(name, elapsed));
		}
		return true;
	}
}