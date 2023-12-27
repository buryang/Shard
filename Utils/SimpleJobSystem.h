#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Memory.h"
#include "Utils/Algorithm.h"
#include "Utils/Hash.h"
#include <thread>
#include <atomic>
#include <optional>

namespace Shard
{
        namespace Utils
        {
                struct JobEntry
                {
                        using JobDeAllocator = std::function<void(JobEntry*)>;
                        mutable std::atomic<uint32_t>        ref_count_{ 1u };
                        std::atomic_bool        is_finished_{ false };
                        JobEntry*        parent_{ nullptr };
                        JobEntry*        subsequent_{ nullptr };
                        //task can stealed by other thread?
                        bool        stealable_{ true };
                        //cpu affinity
                        uint32_t        core_affinity_{ 0xFFFFFFFF };

                        void NotifyFinished() {
                                is_finished_.store(true, std::memory_order_release);
                                is_finished_.notify_one();
                        }
                        void Wait()const{ is_finished_.wait(false, std::memory_order_acquire); }
                        void AddRef()const { ref_count_.fetch_add(1, std::memory_order::relaxed); }
                        void DecRef(){ 
                                ref_count_.fetch_sub(1, std::memory_order::relaxed); 
                                if (!ref_count_) {
                                        auto deallocator = GetDeAllocator();
                                        deallocator(this);
                                }
                        }
                        bool IsFinished() const { return is_finished_.load(); }
                        bool IsStealAble()const { return stealable_; }
                        virtual void operator()(void) = 0;
                        virtual void Release() = 0;
                        virtual constexpr bool IsCoro()const { return false; }
                        virtual JobDeAllocator GetDeAllocator() const { //todo
                                return [](JobEntry* job) {
                                        delete job;
                                };
                        }
                };

                class CoroPromiseBase;
                template<typename Handle>
                class TJobEntry : public JobEntry
                {
                public:
                        TJobEntry() = default;
                        explicit TJobEntry(Handle&& handle) {
                                if constexpr (!std::is_same_v<CoroPromiseBase, std::decay_t<Handle>> && std::is_invocable_v<Handle>) {
                                        new(handle_)Handle(std::move(handle));
                                }
                        }
                        void operator()(void) override {
                                if constexpr (std::is_invocable_v<Handle>) {
                                        (*reinterpret_cast<Handle*>handle_)();
                                }
                                if constexpr (std::is_same_v<CoroPromiseBase, std::decay_t<Handle>>) {
                                        static_cast<CoroPromiseBase*>(this)->resume();
                                }
                                NotifyFinished();
                        }
                        template<typename ...ARGS>
                        void* operator new(std::size_t size, ARGS&&... args) {
                                auto* allocator = TLSScalablePoolAllocatorInstance<uint8_t, POOL_JOBSYSTEM_ID>();
                                const auto alloc_size = size + sizeof(std::uintptr_t);
                                auto* ptr = allocator->allocate(alloc_size);
                                *reinterpret_cast<std::uintptr_t*>(ptr) = allocator; //record the allocator of this thread
                                return ptr + sizeof(std::uintptr_t);
                        }
                        void operator delete(void* ptr, std::size_t size) {
                                auto* raw_ptr = (uint8_t*)(ptr)-sizeof(std::uintptr_t);
                                auto* allocator = reinterpret_cast<ScalablePoolAllocator<uint8_t>*>(raw_ptr); //will release in another thread...
                                allocator->deallocate(raw_ptr, size + sizeof(std::uintptr_t));
                        }
                private:
                        alignas(Handle) uint8_t handle_[sizeof(Handle)]; //for this donot need default constructor
                };


                template<typename T>
                class JobEntryStorage
                {
                public:
                        using size_type = Vector<T>::size_type;
                public:
                        void Init(size_t capacity) {
                                container_.resize(capacity);
                        }
                        T Get(size_type index) {
                                return container_[index];
                        }
                        bool Set(size_type index, T val) {
                                std::swap(container_[index], val);
                                return true;
                        }
                        ~JobEntryStorage() = default;
                private:
                        Vector<T>        container_;
                };

                using JobRingBuffer = TRingBuffer<JobEntry*, JobEntryStorage>;

                class MINIT_API SimpleJobSystem
                {
                public:
                        static SimpleJobSystem& Instance();
                        void Init(const uint32_t group_count, const uint32_t queue_size, const float tls_ratio = 0.5f, bool use_dedicate_core = false);
                        void UnInit();
                        bool Execute(JobEntry* job);
                        /*
                        * A child that finished calls this function for its parent, thus decreasing
                        * the number of left children by one.
                        */
                        bool ChildFinished(JobEntry* job);
                        JobEntry* GetCurrentJob();
                        uint32_t GetCurrentGroupID()const;
                        ~SimpleJobSystem() { UnInit(); }
                private:
                        SimpleJobSystem(const uint32_t try_count=20) : max_try_count_(try_count) {}
                        DISALLOW_COPY_AND_ASSIGN(SimpleJobSystem);
                        void OnFinish(JobEntry* job);
                private:
                        SmallVector<JobRingBuffer>        local_queues_;
                        SmallVector<JobRingBuffer>        global_queues_;
                        SmallVector<std::jthread>        thread_pool_;
                        SmallVector<uint32_t>        thread_affinity_;
                        const uint32_t        max_try_count_{ 15u };
                };

                template<typename F>
                requires std::is_convertible_v<std::decay_t<F>, std::function<void(void)>>
                JobEntry* Schedule(F&& function, JobEntry* parent = nullptr, uint32_t affinity = 0xFFFFFFFF, bool stealable = true)
                {
                        //fixme life time of task and entry
                        JobEntry* job_entry = new TJobEntry<F>(std::forward<F>(function));
                        job_entry->parent_ = parent;
                        job_entry->subsequent_ = nullptr;
                        job_entry->core_affinity_ = affinity;
                        job_entry->stealable_ = stealable;
                        SimpleJobSystem::Instance().Execute(job_entry);
                        return job_entry;
                }

                template<typename F>
                requires std::is_convertible_v<std::decay_t<F>, std::function<void(void)>>
                JobEntry* Continuation(F&& function) {
                        auto* parent = SimpleJobSystem::Instance().GetCurrentJob();
                        if (nullptr != parent && !parent->IsCoro()) {
                                JobEntry* job_entry = new TJobEntry<F>(std::forward<F>(function));
                                parent->subsequent_ = job_entry;
                                return job_entry;
                        }
                        return nullptr;
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

