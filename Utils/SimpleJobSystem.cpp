#include "Utils/SimpleJobSystem.h"

namespace MetaInit
{
	namespace Utils
	{
		SimpleJobSystem& SimpleJobSystem::Instance()
		{
			static SimpleJobSystem js;
			return js;
		}

		void SimpleJobSystem::Init(const uint32_t group_count, const uint32_t queue_size)
		{	
			//ugly only config once time
			static std::atomic<bool> initable{ false };
			bool expected = false;
			if (!initable.compare_exchange_weak(expected, true,std::memory_order::memory_order_acq_rel)) {
				return;
			}

			const auto thread_loop = [&, this](const uint32_t group_id) {
				while (!is_terminated_.load(std::memory_order::memory_order_acquire))
				{
					auto curr_job = local_queues_[group_id].PopFront();
					if (!curr_job.has_value())
					{
						curr_job = global_queues_[group_id].PopFront();
					}

					auto try_count = max_try_count_;
					while (!curr_job.has_value() && try_count > 0)
					{
						curr_job = global_queues_[try_count-- % thread_pool_.size()].PopFront();
					}

					if (curr_job.has_value())
					{
						curr_job.value();
					}
					else
					{
						//if no task input, sleep 5ms
						std::this_thread::sleep_for(std::chrono::milliseconds(5));
					}

				}
			};

			auto thread_count = std::max(group_count, std::thread::hardware_concurrency());

			//init job queue
			for (auto n = 0; n < thread_count; ++n)
			{
				local_queues_.emplace_back(RingBuffer<JobEntry*>(queue_size));
				global_queues_.emplace_back(RingBuffer<JobEntry*>(queue_size));
			}
			//init thread pool
			thread_pool_.reserve(thread_count);
			for (auto n = 0; n < thread_count; ++n)
			{
				thread_pool_.push_back(std::thread(thread_loop));
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
		}

		void SimpleJobSystem::Execute(JobEntry* job)
		{
			auto group_id = std::rand() % thread_pool_.size();

			//FIXME put into local/global queue?
			if (job->stealable_)
			{
				global_queues_[group_id].PushBack(job);
			}
			else
			{
				local_queues_[group_id].PushBack(job);
			}
		}


}