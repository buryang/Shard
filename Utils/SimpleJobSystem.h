#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Memory.h"
#include "Utils/Algorithm.h"
#include <thread>
#include <atomic>
#include <optional>

namespace Shard
{
	namespace Utils
	{
		struct JobEntry
		{
			struct Task
			{
				virtual void operator()(void) = 0;
			};
			template<typename Handle>
			struct TaskEntry final : public Task
			{
				Handle		handle_;
				TaskEntry() = default;
				TaskEntry(Handle&& handle) : handle_(handle) {}
				void operator()(void) override {
					handle_();
				}
			};
			std::atomic<uint32_t>	ref_count_{ 0u };
			std::atomic_bool	is_finished_{ false };
			JobEntry*	parent_{ nullptr };
			//task can stealed by other thread?
			bool	stealable_{ true };
			Task*	task_{ nullptr };
			//cpu affinity
			uint32_t	core_affinity_{ 0xFFFFFFFF };

			void operator()(void) {
				if (nullptr != task_) {
					(*task_)();
				}
				is_finished_.store(true, std::memory_order_release);
				is_finished_.notify_one();
			}
			void Wait();
			void Release();
			bool IsStealAble()const { return stealable_; }
			template<class T>
			requires std::is_base_of<Task, std::decay_t<T>>::value
			void SetTask(T* task) {
				task_ = static_cast<Task*>(task);
			}
			virtual ~JobEntry() = default;
		};

		class JobEntryStorage
		{
		public:
			void Init(size_t capacity);
			JobEntry* Get(size_t index);
			bool Set(size_t index, JobEntry* job);
			~JobEntryStorage() {}
		private:
			Vector<JobEntry*>	jobs_;
		};

		using JobRingBuffer = TRingBuffer<JobEntry*, JobEntryStorage>;

		class MINIT_API SimpleJobSystem
		{
		public:
			SimpleJobSystem& Instance();
			void Init(const uint32_t group_count, const uint32_t queue_size, bool use_dedicate_core = false);
			void UnInit();
			JobEntry* NewJobEntry();
			bool Execute(JobEntry* job);
			~SimpleJobSystem() { UnInit(); }
		private:
			SimpleJobSystem(const uint32_t try_count=20) : max_try_count_(try_count) {}
			DISALLOW_COPY_AND_ASSIGN(SimpleJobSystem);
		private:
			SmallVector<JobRingBuffer>	local_queues_;
			SmallVector<JobRingBuffer>	global_queues_;
			Vector<std::thread>	thread_pool_;
			Vector<uint32_t>	thread_affinity_;
			std::atomic<bool>	is_terminated_{ false };
			const uint32_t	max_try_count_{ 15u };
			PooledAllocator<JobEntry>	allocator_; //FIXME
		};

		template<typename F>
		requires std::is_convertible_v<std::decay_t<F>, std::function<void(void)>>
		JobEntry* Schedule(F&& function, JobEntry* parent = nullptr, uint32_t affinity = 0xFFFFFFFF, bool stealable = true)
		{
			//fixme life time of task and entry
			JobEntry::TaskEntry<F> job_task{ function };
			JobEntry* job_entry = SimpleJobSystem::Instance().NewJobEntry();
			job_entry->SetTask(job_task);
			job_entry->parent_ = parent;
			job_entry->core_affinity_ = affinity;
			job_entry->stealable_ = stealable;
			SimpleJobSystem::Instance().Execute(job_entry);
			return job_entry;
		}

		template<typename F>
		requires std::is_convertible_v<std::decay_t<F>, std::function<void(uint32_t, uint32_t)>>
		uint32_t Dispatch(F&& function, uint32_t group_count, uint32_t thread_count, bool stealable = true) 
		{
			for (auto grp = 0u; grp < group_count; ++grp) {
				for (auto th = 0u; th < thread_count; ++th) {
					auto bind_function = std::bind(function, grp, th);
					Schedule(bind_function, nullptr, 0xFFFFFFFF, stealable);
				}
			}
			return 0u;
		}
	}
}

