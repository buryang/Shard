#pragma once
#include "Utils/CommonUtils.h"
#include <chrono>
#include <shared_mutex>

namespace Shard::Utils
{
	class Timer
	{
	public:
        enum class ETimerType
        {
            eClock,
            eSeconds,
            eMilliSeconds,
        };
		FORCE_INLINE void Record() {
			time_stamp_ = std::chrono::high_resolution_clock::now();
		}
        template<ETimerType time_type=ETimerType::eSeconds>
        FORCE_INLINE auto Elapsed() const {
            if constexpr (time_type == ETimerType::eSeconds) {
                return std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - time_stamp_).count();
            }
            else if constexpr (time_type == ETimerType::eMilliSeconds) {
                return std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - time_stamp_).count();
            }
            return (std::chrono::high_resolution_clock::now() - time_stamp_).count();
        }
		virtual ~Timer() = default;
	private:
		std::chrono::high_resolution_clock::time_point	time_stamp_;
	};

    //floating-point clocks are usually only used to store relatively short time
	class ScopedTimer : public Timer
	{
	public:
		ScopedTimer(double& delta_time):delta_time_(delta_time){
			Record();
		}
		~ScopedTimer() {
			delta_time_ = Elapsed();
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
			const auto elapsed = Timer::Elapsed();
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
