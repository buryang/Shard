#pragma once
#include "Utils/CommonUtils.h"
#include <chrono>
#include <shared_mutex>

namespace Shard::Utils
{
	class Timer
	{
	public:
		FORCE_INLINE void Record() {
			time_stamp_ = std::chrono::steady_clock::now();
		}
		FORCE_INLINE double ElapsedNanoSeconds() const{
			return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - time_stamp_).count();
		}
		FORCE_INLINE double ElapsedMilliSeconds() const{
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - time_stamp_).count();
		}
		virtual ~Timer() = default;
	private:
		std::chrono::steady_clock::time_point	time_stamp_;
	};

	class ScopedTimer : public Timer
	{
	public:
		ScopedTimer(double& delta_time):delta_time_(delta_time){
			Record();
		}
		~ScopedTimer() {
			delta_time_ = ElapsedMilliSeconds();
		}
	private:
		double& delta_time_;
	};

	class Profiler :public Timer
	{
	public:
		static Map<String, std::atomic<double> >	record_profilers_;
		static std::shared_mutex regist_mutex_;
		static void GetAllProfilers(Map<String, double>& profilers);
		static bool RegistProfile(const String& name, double elapsed);
	public:
		Profiler(const String& tag = "anonymous") :tag_(tag) {
			Timer::Record();
		}
		~Profiler() {
			const auto elapsed = Timer::ElapsedMilliSeconds();
			RegistProfile(tag_, elapsed);
		}
		FORCE_INLINE const String& GetName()const {
			return tag_;
		}
	private:
		const String& tag_;
	};

#ifdef TIMER_PROFILER
#define PROFILER(TAG) Profiler timer_##TAG(#TAG);
#else
#define PROFILER(TAG)
#endif

}
