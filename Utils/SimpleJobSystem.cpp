#include "Utils/Platform.h"
#include "Utils/SimpleJobSystem.h"

namespace Shard
{
	namespace Utils
	{
		static constexpr auto MAX_PER_THREAD_JOBS = 256u;

		SimpleJobSystem& SimpleJobSystem::Instance()
		{
			static SimpleJobSystem js;
			return js;
		}

		void SimpleJobSystem::Init(const uint32_t group_count, const uint32_t queue_size, bool use_dedicate_core)
		{	
			//ugly only config once time
			static std::atomic<bool> initable{ false };
			bool expected = false;
			if (!initable.compare_exchange_weak(expected, true, std::memory_order::memory_order_acq_rel)) {
				return;
			}

			const auto thread_loop = [&, this](const uint32_t group_id) {
				if (use_dedicate_core)
				{
					auto& thread_handle = thread_pool_[group_id];
					BindThreadToCPU(thread_handle, group_id);
					//todo thread affinity
					thread_affinity_[group_id] = GetCPUBindToThread(thread_handle);
				}
				while (!is_terminated_.load(std::memory_order::memory_order_acquire))
				{
					JobEntry* curr_job = nullptr;
					auto ret = local_queues_[group_id].Poll(curr_job);
					if (!ret)
					{
						ret = global_queues_[group_id].Poll(curr_job);
					}

					auto try_count = max_try_count_;
					while (!ret && try_count >= 0)
					{
						auto tid = try_count % thread_pool_.size();
						ret = global_queues_[tid].Poll(curr_job);
						//todo thread affinity check 
						//maybe not need this; just do task in affinity ....
						if(use_dedicate_core && ret){
							if (curr_job->core_affinity_ & thread_affinity_[group_id] == 0u) {
								global_queues_[tid].Offer(curr_job); //reoffer to old queue
								ret = false;
							}
						}
						try_count--;
					}

					if (ret)
					{
						curr_job->operator()();
					}
					else
					{
						//if no task input, sleep 5ms
						std::this_thread::sleep_for(std::chrono::milliseconds(5));
					}
				}

				//to do unfinish jobs ?
			};

			auto thread_count = std::clamp(group_count, 1u, std::thread::hardware_concurrency()-1);

			allocator_.Init(thread_count * MAX_PER_THREAD_JOBS);
			//init job queue
			for (auto n = 0; n < thread_count; ++n)
			{
				local_queues_.emplace_back(JobRingBuffer(queue_size));
				global_queues_.emplace_back(JobRingBuffer(queue_size));
			}
			//init thread pool
			if (use_dedicate_core) {
				thread_affinity_.resize(thread_count);
			}
			thread_pool_.reserve(thread_count);
			for (auto n = 0; n < thread_count; ++n)
			{
				thread_pool_.push_back(std::thread(thread_loop, n));
				thread_pool_.back().detach();
			}
		}

		void Utils::SimpleJobSystem::UnInit()
		{
			is_terminated_.store(true, std::memory_order::memory_order_release);

			for (auto& th:thread_pool_)
			{
				th.join();
			}
			//allocator_.UnInit();
		}

		JobEntry* SimpleJobSystem::NewJobEntry()
		{
			allocator_.Alloc();
		}

		bool SimpleJobSystem::Execute(JobEntry* job)
		{
			bool ret{ false };
			SmallVector<uint32_t> affinity_groups;
			for (auto n = 0; n < thread_pool_.size(); ++n) {
				if (thread_affinity_.empty() || thread_affinity_[n]&job->core_affinity_) {
					affinity_groups.emplace_back(n);
				}
			}

			for (auto try_count = 0u; try_count < max_try_count_; ++try_count)
			{
				auto group_id = affinity_groups[std::rand() % affinity_groups.size()];

				//FIXME put into local/global queue?
				if (job->stealable_ && std::rand()%2)
				{
					ret = global_queues_[group_id].Offer(job);
				}
				else
				{
					ret = local_queues_[group_id].Offer(job);
				}
				if (ret) {
					break;
				}
			}
			return ret;
		}

		void JobEntryStorage::Init(size_t capacity)
		{
			jobs_.resize(capacity);
		}

		JobEntry* JobEntryStorage::Get(size_t index)
		{
			return jobs_[index];
		}

		bool JobEntryStorage::Set(size_t index, JobEntry* job)
		{
			std::swap(jobs_[index], job);
			return true;
		}

		void JobEntry::Wait()
		{
			is_finished_.wait(false, std::memory_order_acquire);
		}

}