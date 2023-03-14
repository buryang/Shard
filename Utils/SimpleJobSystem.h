#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Memory.h"
#include <thread>
#include <atomic>
#include <optional>

namespace MetaInit
{
	namespace Utils
	{
		template<typename T>
		class RingBuffer
		{
		public:
			RingBuffer(const uint32_t capacity);
			std::optional<T> PopFront();
			RingBuffer& PushBack(T& data);
			uint32_t Capacity()const;
		private:
			DISALLOW_COPY_AND_ASSIGN(RingBuffer);
			Vector<T>	data_repo_;
			uint32_t	head_{ 0 };
			uint32_t	tail_{ 0 };
			const uint32_t			capacity_;
			std::atomic<bool>		lock_{false};
		};

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
			std::atomic<uint32_t>	ref_count_{ 0 };
			JobEntry*				parent_{ nullptr };
			//task can streal by other thread?
			bool					stealable_{ false };
			Task*					task_{ nullptr };

			void operator()(void) {
				if (nullptr != task_) {
					(*task_)();
				}
			}
			void Release();
			bool IsStealAble() { return stealable_; }
			template<class T>
			requires std::is_base_of<Task, std::decay_t<T>>::value
			void SetTask(T* task) {
				task_ = static_cast<Task*>(task);
			}
			virtual ~JobEntry() {};
		};

		class MINIT_API SimpleJobSystem
		{
		public:
			SimpleJobSystem& Instance();
			void Init(const uint32_t group_count, const uint32_t queue_size);
			void UnInit();
			void Dispatch();
			void Execute(JobEntry* job);
			~SimpleJobSystem() { UnInit(); }
		private:
			SimpleJobSystem(const uint32_t try_count=20) : max_try_count_(try_count) {}
			DISALLOW_COPY_AND_ASSIGN(SimpleJobSystem);
		private:
			SmallVector<RingBuffer<JobEntry*> >		local_queues_;
			SmallVector<RingBuffer<JobEntry*> >		global_queues_;
			Vector<std::thread>			thread_pool_;
			std::atomic<bool>			is_terminated_{ false };
			const uint32_t				max_try_count_{ 15 };
			Allocator					allocator_; //FIXME
		};

		template<typename F>
		requires std::is_convertible_v<std::decay_t<F>, std::function<void(void)>>
		uint32_t Schedule(F&& function, JobEntry* parent = nullptr)
		{
			//fixme life time of task and entry
			JobEntry::TaskEntry<F> job_task{ function };
			JobEntry job_entry;
			job_entry.SetTask(job_task);
			job_entry.parent_ = parent;
			SimpleJobSystem::Instance().Execute(job_entry);
			return 0u;
		}
	}
}

