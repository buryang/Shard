#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Memory.h"
#include "Utils/Algorithm.h"
#include "Utils/Hash.h"
#include "Utils/Platform.h"
#include <thread>
#include <atomic>
#include <optional>

//todo job graph and job priority 

/* if we use WITH_JOB_SYSTEM_AFFINITY, the engine is like old system-on-a-thread design. 
 *  for cpu per-thread load balance, we'd better use WITH_JOB_SYSTEM_PRIORITY
 */
#define JOB_SYSTEM_AFFINITY_ENABLED 
#ifndef JOB_SYSTEM_AFFINITY_ENABLED
#define JOB_SYSTEM_PRIORITY_ENABLED
#endif

namespace Shard
{
    namespace Utils
    {
        struct JobEntry
        {
            using JobDeAllocator = std::function<void(JobEntry*)>;
            //std::atomic<uint32_t>   counter_{ 0u }; job dependency counter
            mutable std::atomic<uint32_t>    ref_count_{ 1u }; //default 1, for jobsystem to release it after job finished
            std::atomic<bool>    completed_{ false };
            JobEntry*    parent_{ nullptr };
            JobEntry*    subsequent_{ nullptr };
            //task can stealed by other thread?
            bool    stealable_{ true };
            //cpu affinity
            uint32_t    core_affinity_{ 0xFFFFFFFF };

            void Init(JobEntry* parent, bool stealable, uint32_t core_affinity) {
                parent_ = parent;
                stealable_ = stealable;
                core_affinity_ = core_affinity;
                ref_count_.store(1u, std::memory_order::relaxed);
                completed_.store(false, std::memory_order::relaxed);
                subsequent_ = nullptr;

                if (nullptr != parent) {
                    parent_->AddRef();
                }
            }

            void AddRef()const { ref_count_.fetch_add(1u, std::memory_order::release); }
            void DecRef(){ 
                ref_count_.fetch_sub(1u, std::memory_order::release); 
                if (!ref_count_.load(std::memory_order::acquire)) {
                    /*for coroutine call promise'operator new to allocate promise state, so use"delete this" is error*/
                    auto deallocator = GetDeAllocator();
                    deallocator(this);

                    //call parent's DecRef
                    if (nullptr != parent_) {
                        parent_->DecRef();
                    }
                }
            }
            bool IsStealAble()const { return stealable_; }
            virtual void operator()(void) = 0;
            virtual void Release() = 0;
            virtual constexpr bool IsCoro()const { return false; }
            virtual JobDeAllocator GetDeAllocator() const = 0;
        };

        /**
         * \relates todo realize job counter like "the job system in cuberpunk 2077"
         */
        /*
        struct CounterEntry
        {
            std::atomic<uint32_t>   value_{ 0u };
            mutable std::atomic<uint32_t>   ref_count_{ 1u };
            Utils::IntrusiveLinkedList<Utils::DefaultIntrusiveListTraits<JobEntry>> waiting_jobs_; //jobs waiting on this counter

            //overload new and delete
            template<typename ...ARGS>
            void* operator new(std::size_t size, ARGS&&... args) {
                auto* allocator = TLSScalablePoolAllocatorInstance<uint8_t, POOL_JOBSYSTEM_ID>();
                const auto alloc_offset = AlignUp(size, alignof(std::uintptr_t));
                auto* ptr = allocator->allocate(alloc_offset + sizeof(std::uintptr_t));
                *reinterpret_cast<std::uintptr_t*>(ptr + alloc_offset) = (std::uintptr_t)allocator; //record the allocator of this thread
                return ptr;
            }
            void operator delete(void* ptr, std::size_t size) {
                const auto alloc_offset = AlignUp(size, alignof(std::uintptr_t));
                auto raw_ptr = std::uintptr_t(ptr) + alloc_offset;
                auto* allocator = (ScalablePoolAllocator<uint8_t>*)(*reinterpret_cast<std::uintptr_t*>(raw_ptr)); //will release in another thread...
                allocator->deallocate((uint8_t*)raw_ptr, alloc_offset + sizeof(std::uintptr_t));
            }
        };
        */

        class TJobEntry final: public JobEntry
        {
        private:
            std::function<void(void)>   handle_;
        public:
            TJobEntry() = default;
            template<typename Handle>
            requires(std::is_invocable_v<Handle>)
            explicit TJobEntry(Handle&& handle) {
                handle_ = std::forward<Handle>(handle);
            }
            virtual ~TJobEntry() = default;
            void operator()(void) override {
                handle_();
                completed_.store(true,  std::memory_order::release);
            }
            void Release() override {

            }

            JobDeAllocator GetDeAllocator() const final
            {
                return [](JobEntry* job) {
                    auto* tjob = static_cast<TJobEntry*>(job);
                    assert(tjob != nullptr);
                    delete tjob;
                };
            }

            template<typename ...ARGS>
            void* operator new(std::size_t size, ARGS&&... args) {
                auto* allocator = TLSScalablePoolAllocatorInstance<uint8_t, POOL_JOBSYSTEM_ID>();
                const auto alloc_offset = AlignUp(size, alignof(std::uintptr_t));
                auto* ptr = allocator->allocate(alloc_offset + sizeof(std::uintptr_t));
                *reinterpret_cast<std::uintptr_t*>(ptr + alloc_offset) = (std::uintptr_t)allocator; //record the allocator of this thread
                return ptr;
            }
            void operator delete(void* ptr, std::size_t size) {
                const auto alloc_offset = AlignUp(size, alignof(std::uintptr_t));
                auto raw_ptr = std::uintptr_t(ptr)+alloc_offset;
                auto* allocator = (ScalablePoolAllocator<uint8_t>*)(*reinterpret_cast<std::uintptr_t*>(raw_ptr)); //will release in another thread...
                allocator->deallocate((uint8_t*)raw_ptr, alloc_offset + sizeof(std::uintptr_t));
            }
        };

        template<typename T>
        class JobEntryStorage
        {
        public:
            using size_type = Vector<T>::size_type;
        public:
            void Init(size_t capacity) {
                Resize(capacity);
            }
            T Get(size_type index) {
                return container_[index];
            }
            bool Set(size_type index, T val) {
                std::swap(container_[index], val);
                return true;
            }
            void Resize(size_type sz) {
                container_.resize(sz);
            }
            ~JobEntryStorage() = default;
        private:
            ALIGN_CACHELINE SmallVector<T>   container_;
        };

        using JobRingBuffer = TRingBuffer<JobEntry*, nullptr, JobEntryStorage>;

        class MINIT_API SimpleJobSystem
        {
        public:
            static SimpleJobSystem& Instance();
            /**
             * \param group_count: thread count
             * \param queue_size: job queue size
             * \param use_dedicate_core: whether each thread use dedicate core
             */
            void Init(const uint32_t group_count, const uint32_t queue_size, bool use_dedicate_core = false);
            /**
             * \param flash : flash jobs in queue before unint
             */
            void UnInit(bool flash = true);
            /**
             * \param job: job to execute
             * \return: return true if job enqueue successfully 
             */
            bool Execute(JobEntry* job);
            /**
            * A child that finished calls this function for its parent, thus decreasing
            * the number of left children by one.
            */
            bool ChildFinished(JobEntry* job);
            /*return nullptr for non jobsystem thread*/
            JobEntry* GetCurrentJob();
            uint32_t GetCurrentGroupID()const;
            ~SimpleJobSystem() { UnInit(); }
        private:
            SimpleJobSystem(const uint32_t try_count=20) : max_try_count_(try_count) {}
            DISALLOW_COPY_AND_ASSIGN(SimpleJobSystem);
            void OnFinish(JobEntry* job);
        private:
            SmallVector<JobRingBuffer>  local_queues_;
            SmallVector<JobRingBuffer>  global_queues_;
            SmallVector<std::jthread>   thread_pool_;
            SmallVector<float>          thread_priority_;
            SmallVector<uint32_t>       thread_affinity_;
            const uint32_t    max_try_count_{ 15u };
        };

        /**
         * \brief class to wait/check job complete status out of job system
         * should be constructed before job submitted to job system
         */
        class JobHandle
        {
            JobEntry* job_{ nullptr };
        public:

            JobHandle(JobEntry& job) : job_(job) {
                if (nullptr != job_) {
                    job_->AddRef();
                }
            }
            JobHandle(const JobHandle& rhs) : job_(rhs.job_) {
                if (nullptr != job_) {
                    job_->AddRef();
                }
            }
            JobHandle(JobHandle&& rhs) noexcept : job_(rhs.job_) {
                rhs.job_ = nullptr;
            }
            auto& operator=(const JobHandle& rhs) {
                Reset();
                job_ = rhs.job_;
                if (nullptr != job_) {
                    job_->AddRef();
                }
                return *this;
            }
            auto& operator=(JobHandle&& rhs) noexcept {
                Reset();
                job_ = rhs.job_;
                return *this;
            }
            ~JobHandle() {
                Reset();
            }
            operator bool()const {
                return nullptr != job_;
            }
            auto& operator*() {
                return *job_;
            }
            const auto& operator*() const {
                return *job_;
            }
            auto* operator->() {
                return job_;
            }
            const auto* operator->() const {
                return job_;
            }
            void Reset() {
                job_->DecRef();
                job_ = nullptr;
            }

            bool TryWait() {
                return job_ && job_->completed_.load(std::memory_order_acquire);
            }

            bool IsDone() const {
                return job_ && job_->completed_.load(std::memory_order_acquire);
            }

            void Wait() {
                if (!job_) return;
                while (!job_->completed_.load(std::memory_order_acquire)) //wait until job done
                {
                    mm_pause();
                }
            }
        };

        template<typename F>
        requires std::is_convertible_v<std::decay_t<F>, std::function<void(void)>>
        JobHandle Schedule(F&& function, JobEntry* parent = SimpleJobSystem::Instance().GetCurrentJob(), uint32_t affinity = 0xFFFFFFFF, bool stealable = true) //parent default value
        {
            JobEntry* job_entry{ new TJobEntry(std::forward<F>(function)) };
            job_entry->Init(parent, stealable, affinity);
        
            JobHandle job_handle(job_entry); //initial handle before submit to job system
            SimpleJobSystem::Instance().Execute(job_entry);
            return job_handle;
        }

        template<typename F>
        requires std::is_convertible_v<std::decay_t<F>, std::function<void(void)>>
        bool Continuation(F&& function) {
            auto* parent = SimpleJobSystem::Instance().GetCurrentJob();
            if (nullptr != parent && !parent->IsCoro()) {
                JobEntry* job_entry = new TJobEntry<F>(std::forward<F>(function));
                parent->subsequent_ = job_entry;
                return true;
            }
            //job will be submitted while parent job done
            return false;
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

