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
            mutable std::atomic<uint32_t>    ref_count_{ 1u };
            JobEntry*    parent_{ nullptr };
            JobEntry*    subsequent_{ nullptr };
            //task can stealed by other thread?
            bool    stealable_{ true };
            //cpu affinity
            uint32_t    core_affinity_{ 0xFFFFFFFF };

            void AddRef()const { ref_count_.fetch_add(1u, std::memory_order::release); }
            void DecRef(){ 
                ref_count_.fetch_sub(1u, std::memory_order::release); 
                if (!ref_count_.load(std::memory_order::acquire)) {
                    /*for coroutine call promise'operator new to allocate promise state, so use"delete this" is error*/
                    auto deallocator = GetDeAllocator();
                    deallocator(this);
                  
                }
            }
            bool IsStealAble()const { return stealable_; }
            virtual void operator()(void) = 0;
            virtual void Release() = 0;
            virtual constexpr bool IsCoro()const { return false; }
            virtual JobDeAllocator GetDeAllocator() const = 0;
        };

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

        using JobRingBuffer = TRingBuffer<JobEntry*, JobEntryStorage>;

        class MINIT_API SimpleJobSystem
        {
        public:
            static SimpleJobSystem& Instance();
            void Init(const uint32_t group_count, const uint32_t queue_size, bool use_dedicate_core = false);
            void UnInit();
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
            SmallVector<JobRingBuffer>    local_queues_;
            SmallVector<JobRingBuffer>    global_queues_;
            SmallVector<std::jthread>    thread_pool_;
            SmallVector<uint32_t>    thread_affinity_;
            const uint32_t    max_try_count_{ 15u };
        };

        template<typename F>
        requires std::is_convertible_v<std::decay_t<F>, std::function<void(void)>>
        void Schedule(F&& function, JobEntry* parent = SimpleJobSystem::Instance().GetCurrentJob(), uint32_t affinity = 0xFFFFFFFF, bool stealable = true) //parent default value
        {
            JobEntry* job_entry{ new TJobEntry(std::forward<F>(function)) };
            job_entry->parent_ = parent;
            job_entry->subsequent_ = nullptr;
            job_entry->core_affinity_ = affinity;
            job_entry->stealable_ = stealable;
            SimpleJobSystem::Instance().Execute(job_entry);
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

