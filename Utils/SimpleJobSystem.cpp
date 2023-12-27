#include <thread>
#include "Utils/CommonUtils.h"
#include "Utils/Platform.h"
#include "Utils/SimpleJobSystem.h"

namespace Shard
{
    namespace Utils
    {
        static constexpr auto MAX_PER_THREAD_JOBS = 64u;

        //storage for coro frame/job entity here
        namespace {
            ALIGN_CACHELINE SimpleJobSystem g_job_sys;
            /*real application spend up to 20% of execution time in memory allocator routines, can never
            achive more than a 5x speedup*/
        }

        namespace TLS
        {
            thread_local JobEntry* g_current_job{ nullptr }; //atomic operator
            thread_local uint32_t g_current_gid{ -1 };
        }

        SimpleJobSystem& SimpleJobSystem::Instance()
        {
            return g_job_sys;
        }

        void SimpleJobSystem::Init(const uint32_t group_count, const uint32_t queue_size, const float tls_ratio, bool use_dedicate_core)
        {    
            //ugly only config once time
            static std::atomic<bool> initable{ false };
            bool expected = false;
            if (!initable.compare_exchange_weak(expected, true, std::memory_order::memory_order_acq_rel)) {
                return;
            }

            const auto thread_loop = [&, this](std::stop_token stop_token, const uint32_t group_id) {
                if (use_dedicate_core)
                {
                    auto& thread_handle = thread_pool_[group_id];
                    BindThreadToCPU(thread_handle, group_id);
                    //todo thread affinity
                    thread_affinity_[group_id] = GetCPUBindToThread(thread_handle);
                }
                //set thread local resource
                TLS::g_current_gid = group_id;
                while (!stop_token.stop_requested())
                {
                    TLS::g_current_job = nullptr;
                    auto ret = local_queues_[group_id].Poll(TLS::g_current_job);
                    if (!ret)
                    {
                        ret = global_queues_[group_id].Poll(TLS::g_current_job);
                    }

                    auto try_count = max_try_count_;
                    while (!ret && try_count >= 0)
                    {
                        auto tid = try_count % thread_pool_.size();
                        ret = global_queues_[tid].Poll(TLS::g_current_job);
                        //todo thread affinity check 
                        //maybe not need this; just do task in affinity ....
                        if (use_dedicate_core && ret) {
                            if ((TLS::g_current_job->core_affinity_ & thread_affinity_[group_id]) == 0u) {
                                global_queues_[tid].Offer(TLS::g_current_job); //reoffer to old queue
                                ret = false;
                            }
                        }
                        try_count--;
                        _mm_pause();//
                    }

                    if (ret)
                    {
                        TLS::g_current_job->operator()();

                        //finish job,release resource 
                        const auto is_coro = TLS::g_current_job->IsCoro();
                        if (!is_coro) {
                            assert(TLS::g_current_job->ref_count_.load() == 1);
                            ChildFinished(TLS::g_current_job);
                        }
                    }
                    else[[unlikely]]
                    {
                        //if no task input, yield
                        std::this_thread::yield();
                        
                    }
                }

            };

            const auto thread_count = std::clamp(group_count, 1u, std::thread::hardware_concurrency()-1);

            //init thread local storage
            const auto tls_count = std::max(1u, uint32_t(thread_count * tls_ratio));

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
                thread_pool_.emplace_back(std::jthread(thread_loop, n));
                thread_pool_.back().detach();
            }
        }

        void SimpleJobSystem::UnInit()
        {
#if 0
            for (auto& th:thread_pool_)
            {
                th.request_stop();
                th.join();
            }
#else
            for (auto n = 0; n < thread_pool_.size(); ++n) {
                //todo: add terminate function to each thread to finish current jobs
                auto terminate = [&, this]() {
                    auto& th = thread_pool_[n]; 
                    assert(std::this_thread::get_id() == th.get_id()); //kill current thread
                    th.request_stop();
                };
                Schedule(terminate, nullptr, n, false);
            }

            for (auto& th : thread_pool_) {
                th.join();
            }
#endif
            //allocator_.UnInit();
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

        bool SimpleJobSystem::ChildFinished(JobEntry* job)
        {

            if (job->ref_count_.load() == 1u) {
                if (!job->IsCoro()) {
                    OnFinish(job);
                }
                else
                {
                    Execute(job);
                }
                return true;
            }
            else
            {
                job->DecRef(); //todo bug
            }
            return false;
        }

        JobEntry* SimpleJobSystem::GetCurrentJob()
        {
            //running in a job entry, so no need to do lock
            return TLS::g_current_job;
        }

        uint32_t SimpleJobSystem::GetCurrentGroupID() const
        {
            return TLS::g_current_gid;
        }

        void SimpleJobSystem::OnFinish(JobEntry* job)
        {
            if (job->subsequent_ != nullptr)
            {
                if (nullptr != job->parent_) {
                    job->parent_->AddRef();
                    job->subsequent_->parent_ = job->parent_;
                }
                SimpleJobSystem::Instance().Execute(job->subsequent_);
            }
            if (nullptr != job->parent_) {
                ChildFinished(job->parent_);
            }
            //delete job;
            job->DecRef();
        }

}