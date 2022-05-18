#include "Utils/SimpleJobSystem.h"

namespace MetaInit
{
	namespace Utils
	{
		void SimpleJobSystem::Init(const uint32_t group_count)
		{	
			const auto thread_loop = [&](const uint32_t group_id) {
				while (!is_terminated_.load())
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
						(*curr_job)();
					}
					else
					{
						//if no task input, sleep 5ms
						std::this_thread::sleep_for(std::chrono::milliseconds(5));
					}

				}
			};
			thread_pool_.reserve(group_count);
			for (auto n = 0; n < group_count; ++n)
			{
				thread_pool_.push_back(std::thread(thread_loop));
			}

		}

		void Utils::SimpleJobSystem::UnInit()
		{
			is_terminated_.store(true);

			for (auto& th:thread_pool_)
			{
				th.join();
			}
		}

		void SimpleJobSystem::Execute(JobEntry* job)
		{
			auto group_id = std::rand() % thread_pool_.size();

			//FIXME put into local/global queue?
			if (job.stealable_)
			{
				global_queues_[group_id].PushBack(job);
			}
			else
			{
				local_queues_[group_id].PushBack(job);
			}
		}


}