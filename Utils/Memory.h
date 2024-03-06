#pragma once
#include "Utils/CommonUtils.h"
//http://dmitrysoshnikov.com/compilers/writing-a-memory-allocator/

namespace Shard
{
    namespace Utils
    {
        class LinearAllocatorImpl final
        {
        public:
            explicit LinearAllocatorImpl(size_type capacity);
            void* allocate(size_type size, size_type n);
            void deallocate(void* ptr, size_type n);
            void Reset();
            const size_type max_size()const {
                return capacity_;
            }
            ~LinearAllocatorImpl();
        private:
            size_type    capacity_{ 0u };
            size_type    offset_{ 0u };
            void* memory_{ nullptr };
        };

        template<class T>
        class LinearAllocator
        {
        public:
            //field for allocator_traits
            using value_type = T;
            using pointer = T*;
            template<class U>
            struct rebind {
                using other = LinearAllocator<U>;
            };
            template<class>
            friend class LinearAllocator;
        public:
            explicit LinearAllocator(size_type capacity = 1024 * 1024) :impl_(std::make_shared<LinearAllocatorImpl>(capacity)){
            }
            template<class U>
            LinearAllocator(const LinearAllocator<U>& other) noexcept : impl_(other.impl_) //todo
            {

            }
            [[nodiscard]] pointer allocate(size_type n = 1u) {
                pointer ptr = impl_->Alloc(sizeof(T), n);
                if (nullptr != ptr) {
                    return reinterpret_cast<T*>(ptr);
                }
                return ptr;
            }
            template <typename... Args>
            [[nodiscard]] pointer AllocNoDestruct(Args&&... args) {
                auto ptr = allocate();
                return new(ptr)T(std::forward<Args>(args)...);
            }
            void deallocate(pointer ptr, size_type n) {
                impl_->DeAlloc(ptr, n * sizeof(T));
            }
            size_type max_size()const {
                return impl_->GetCapacity() / sizeof(value_type);
            }
            ~LinearAllocator() = default;
            void Reset() {
                impl_->Reset();
            }
            template<class U>
            friend bool operator==(const LinearAllocator& lhs, const LinearAllocator<U>& rhs) {
                return lhs.impl_.get() && lhs.impl_ == rhs.impl_;
            }
            template<class U>
            friend bool operator!=(const LinearAllocator& lhs, const LinearAllocator<U>& rhs) {
                return !(lhs == rhs);
            }
        private:
            std::shared_ptr<LinearAllocatorImpl>    impl_;
        };

        class StackAllocatorImpl final
        {
        public:
            StackAllocatorImpl() = default;
            explicit StackAllocatorImpl(size_type capacity);
            void* allocate(size_type size, size_type n);
            void deallocate(void* ptr, size_type n);
            void Reset();
            ~StackAllocatorImpl();
        private:
            size_type    capactity_{ 0u };
            size_type    offset_{ 0u };
            void* memory_{ nullptr };
        };

        template<class T>
        class StackAllocator
        {
        public:
            //field for allocator_traits
            using value_type = T;
            template<class U>
            struct rebind
            {
                using other = StackAllocator<U>;
            };
            template<class>
            friend class StackAllocator;
        public:
            StackAllocator() = default;
            explicit StackAllocator(size_type capacity) :std::make_shared<StackAllocatorImpl>(capacity) {

            }
            template<class U>
            StackAllocator(const StackAllocator<U>& other) noexcept :impl_(other.impl_)
            {
            }
            [[nodiscard]] T* allocate(size_type n = 1u) {
                return reinterpret_cast<T*>(impl_->Alloc(sizeof(T), n));
            }
            template <typename... Args>
            [[nodiscard]] T* AllocNoDestruct(Args&&... args) {
                auto ptr = allocate();
                return new(ptr)T(std::forward<Args>(args)...);
            }
            void deallocate(T* ptr, size_type n) {
                std::destroy(ptr, ptr + n);
                impl_->DeAlloc(ptr, sizeof(T) * n);
            }
            void Reset() {
                impl_->Reset();
            }
            template<class U>
            friend bool operator==(const StackAllocator& lhs, const StackAllocator<U>& rhs) {
                return lhs.impl_.get() && lhs.impl_ == rhs.impl_;
            }
            template<class U>
            friend bool operator!=(const StackAllocator& lhs, const StackAllocator<U>& rhs) {
                return !(lhs == rhs);
            }
        private:
            std::shared_ptr<StackAllocatorImpl> impl_;
        };

        template<class T>
        class DoubleEndStackAllocator
        {
            //todo
        };

        template<typename T>
        class PooledAllocator //not copyable
        {
        public:
            using value_type = T;
        public:
            PooledAllocator() = default;
            explicit PooledAllocator(size_type element_count) {
                Init(element_count);
            }
            void Init(size_type element_count) {
                memory_ = ::operator new(sizeof(Node) * element_count);
                free_head_ = reinterpret_cast<Node*>(memory_);
                auto* tail = free_head_;
                //constructor free list
                for (auto n = 0; n < element_count; ++n, ++tail) {
                    tail->next_ = tail + 1;
                }
                tail->next_ = nullptr;
            }
            //todo lock
            [[nodiscard]] T* allocate() {
                auto* next = free_head_->next_;
                if (next == nullptr) {
                    return nullptr;
                }
                std::swap(next, free_head_);
                return &(next->data_);
            }
            template <typename... Args>
            [[nodiscard]] T* AllocNoDestruct(Args&&... args) {
                auto ptr = allocate();
                return new(ptr)T(std::forward<Args>(args)...);
            }
            void deallocate(T* ptr) {
                std::destroy(ptr, ptr + 1);//to do
                auto* node = reintepret_cast<Node*>(static_cast<uintptr_t>(ptr) - sizeof(Node*));
                node->next_ = free_head_;
                std::swap(node, free_head_);
            }
            ~PooledAllocator() { ::operator delete(memory_); memory_ = nullptr; }
            friend bool operator==(const PooledAllocator& lhs, const PooledAllocator& rhs) {
                return rhs.memory_ == rhs.memory_;
            }
            friend bool operator!=(const PooledAllocator& lhs, const PooledAllocator& rhs) {
                return !(lhs == rhs);
            }
        private:
            DISALLOW_COPY_AND_ASSIGN(PooledAllocator);
        private:
            struct Node {
                Node* next_{ nullptr }; //todo
                T    data_;
            };
            Node* free_head_{ nullptr };
            void* memory_{ nullptr };
        };

        //memory pool allocator from apex.ai
        namespace MemoryPool
        {
            template<size_type id = 0u>
            struct PoolBucketInfo
            {
                using Type = std::tuple<>;
                static constexpr size_type MaxSize = 0u;
            };

            /* todo size from https ://www.bsdcan.org/2006/papers/jemalloc.pdf
            *https://github.com/google/tcmalloc/blob/master/tcmalloc/size_classes.cc
            *https://google.github.io/tcmalloc/design.html for more information
            */
            struct BucketType16
            {
                static constexpr auto BLK_SIZE = 16u;
                static constexpr auto BLK_COUNT = 1000u;
            };

            struct BucketType32
            {
                static constexpr auto BLK_SIZE = 32u;
                static constexpr auto BLK_COUNT = 1000u;
            };

            struct BucketType64
            {
                static constexpr auto BLK_SIZE = 64u;
                static constexpr auto BLK_COUNT = 1000u;
            };

            struct BucketType128
            {
                static constexpr auto BLK_SIZE = 128u;
                static constexpr auto BLK_COUNT = 100u;
            };

            struct BucketType256
            {
                static constexpr auto BLK_SIZE = 256u;
                static constexpr auto BLK_COUNT = 512u;
            };

            struct BucketType512
            {
                static constexpr auto BLK_SIZE = 512u;
                static constexpr auto BLK_COUNT = 100u;
            };

            struct BucketType1024
            {
                static constexpr auto BLK_SIZE = 1024u;
                static constexpr auto BLK_COUNT = 10u;
            };
#define REGIST_POOL_ID(ID) template<> struct PoolBucketInfo<ID> { using Type = std::tuple<BucketType16, BucketType32, BucketType64, BucketType128, BucketType256, BucketType512, BucketType1024>;  static constexpr size_type MaxSize = BucketType1024::BLK_SIZE; };

            //todo defrag ? like https://github.com/jemalloc/jemalloc/issues/566 ??
            class Bucket final
            {
            public:
                explicit Bucket(size_type blk_size, size_type blk_count);
                ~Bucket();
                void* allocate(size_type size);
                void deallocate(void* ptr, size_type size);
                bool IsPointerBelong(void* ptr)const;
                FORCE_INLINE size_type GetBlkSize()const {
                    return blk_size_;
                }
                FORCE_INLINE size_type GetBlkCount()const {
                    return blk_count_;
                }
            private:
                size_type FindContinusBlocks(size_type size) const;
                void SetBlockInUse(size_type index, size_type n);
                void SetBlockFree(size_type index, size_type n);
            private:
                const size_type blk_size_{ 0u };
                const size_type blk_count_{ 0u };
                void* memory_{ nullptr };
                void* ledger_{ nullptr };
            };


            template<size_type id>
            using PoolBucketInfo_Type = PoolBucketInfo<id>::Type;
            template<size_type id>
            struct PoolBucketInfo_TypeCount : std::integral_constant<size_type, std::tuple_size_v<PoolBucketInfo_Type<id>>>{};
            template<size_type id, size_type index>
            struct BucketBlkSize : std::integral_constant<size_type, std::tuple_element_t<index, PoolBucketInfo_Type<id>>::BLK_SIZE> {};
            template<size_type id, size_type index>
            struct BucketBlkCount : std::integral_constant<size_type, std::tuple_element_t<index, PoolBucketInfo_Type<id>>::BLK_COUNT> {};
            template<size_type id>
            struct PoolType : Array<Bucket, PoolBucketInfo_TypeCount<id>::value> {};

            template<size_type id, size_type ...index>
            auto& PoolInstance(std::index_sequence<index...>) noexcept{
                static PoolType<id> instance{ { { BucketBlkSize<id, index>::value, BucketBlkCount<id, index>::value } ...} };
                return instance;
            }

            template<size_type id>
            auto& PoolInstance() noexcept {
                return PoolInstance<id>(std::make_index_sequence<PoolBucketInfo_TypeCount<id>::value>());
            }

            namespace {
                struct Info {
                    size_type    index_{ 0u };
                    size_type    blk_count_{ 0u };
                    size_type    waste_{ 0u };
                };
                bool operator<(const Info& lhs, const Info& rhs) {
                    return (lhs.waste_ == rhs.waste_) ? lhs.blk_count_ < rhs.blk_count_ : lhs.waste_ < rhs.waste_;
                }
            }
            template<size_type id>
            [[nodiscard]] void* allocate(size_type size) {
                if LIKELY(size <= PoolBucketInfo_Type<id>::MaxSize)
                {
                    auto& pool = PoolInstance<id>();
                    Array<Info, PoolBucketInfo_TypeCount<id>> delta;
                    size_type index{ 0u };
                    for (auto& bucket : pool) {
                        if (auto blk_size = bucket.GetBlkSize(); blk_size > size) {
                            delta.emplace_back({ index, 1u, blk_size - size });
                        }
                        else {
                            const auto blk_count = 1u + (size - 1u) / bucket.GetBlkSize();
                            delta.emplace_back({ index, blk_count, blk_count * bucket.GetBlkSize() - size });
                        }
                        ++index;
                    }
                    eastl::heap_sort(delta.begin(), delta.end());
                    for (const auto& dt : delta) {
                        if (auto ptr = pool[dt.index_].Alloc(size); ptr != nullptr) {
                            return ptr;
                        }
                    }
                    return nullptr;
                }
                else
                {
                    return ::operator new(size); //alloc big chunk
                }
            }

            template<size_type id>
            void deallocate(void* ptr, size_type size) {
                if LIKELY(size <= PoolBucketInfo_Type<id>::MaxSize) {
                    auto& pool = PoolInstance<id>();
                    for (auto& bucket : pool) {
                        if (bucket.IsPointerBelong(ptr)) {
                            bucket.deallocate(ptr, size);
                            break;
                        }
                    }
                }
                else
                {
                    ::operator delete(ptr); //free big chunk
                }
            }

            enum {
                SCALABLE_BUCKET_FAKE_ID = 1024,
                SCALEBLE_CHUNK_SIZE = 2 * 1024 * 1024,
            };

            REGIST_POOL_ID(SCALABLE_BUCKET_FAKE_ID); //ugly

            namespace Scalable
            {
                struct FreeBlock
                {
                    FreeBlock* next_{ nullptr };
                };

                struct Chunk
                {
                    uint32_t free_blks_{ 0u };
                    Chunk* prev_{ nullptr };
                    Chunk* next_{ nullptr };
                    FreeBlock* free_list_{ nullptr };
                    std::uintptr_t bump_ptr_{ 0u }; //why not nullptr?
                    void* memory_{ nullptr };
                };


                class ScalableBucket final
                {
                public:
                    friend class ScalablePool;
                    ScalableBucket(size_type blk_size);
                    ScalableBucket(ScalableBucket&& other);
                    [[nodiscard]] void* allocate([[maybe_unused]] size_type size);
                    //default deallocate; do deallcating in this thread
                    void deallocate(void* ptr, [[maybe_unused]] size_type size);
                    //do deallocating in another thread
                    void deallocate_external(void* ptr, [[maybe_unused]] size_type size);
                    ~ScalableBucket();
                private:
                    Chunk* AllocChunk();
                    void DeAllocChunk(Chunk* chunk);
                    void RecyleChunks();
                    bool TryRecyleExternalFrees();
                private:
                    Chunk*    active_chunk_{ nullptr };
                    const size_type    blk_size_{ 0u };
                    const size_type    blk_count_{ 0u };
                    std::atomic<FreeBlock*>    external_freed_{ nullptr };
                };

                class ScalablePool
                {
                public:
                    ScalablePool() noexcept;
                    [[nodiscard]] void* allocate(size_type size);
                    void deallocate(void* ptr, size_type size);
                    void deallocate_external(void* ptr, size_type size);
                private:
                    DISALLOW_COPY_AND_ASSIGN(ScalablePool);
                private:
                    Array<ScalableBucket, PoolBucketInfo_TypeCount<SCALABLE_BUCKET_FAKE_ID>::value>  buckets_;
                };

                template<size_type id = 0u>
                ScalablePool* TLSScalablePoolInstance() {
                    thread_local ScalablePool pool;
                    return &pool;
                }
            }

        }

        //one stateless allocator. static prealloc pool
        template<typename T, size_type id = 0u>
        class StaticPoolAllocator
        {
        public:
            using value_type = T;
            template<typename U>
            struct rebind {
                using other = StaticPoolAllocator<U, id>;
            };
        public:
            StaticPoolAllocator() = default;
            template<typename U>
            StaticPoolAllocator(const StaticPoolAllocator<U, id>& other) {

            }
            [[nodiscard]] T* allocate(size_type n) {
                return static_cast<T*>(MemoryPool::allocate<id>(sizeof(T) * n));
            }
            void deallocate(T* ptr, size_type n) {
                MemoryPool::deallocate<id>(ptr, sizeof(T) * n);
            }
            template<class U>
            friend bool operator==(const StaticPoolAllocator& lhs, const StaticPoolAllocator<U, id>& rhs) {
                return true;
            }
            template<class U>
            friend bool operator!=(const StaticPoolAllocator& lhs, const StaticPoolAllocator<U, id>& rhs) {
                return false;
            }
        };

        enum
        {
            POOL_DEFAULT_ID         = 0x0,
#ifdef USE_UNIFORM_POOL
            POOL_JOBSYSTEM_ID       = POOL_DEFAULT_ID,
            POOL_RHI_ID             = POOL_DEFAULT_ID,
#else
            POOL_JOBSYSTEM_ID       = 0x1,
            POOL_RHI_ID             = 0x2,
#endif
            POOL_NUM,
            //to avoid conflict; you'd better  use 
            //GUID first 32bit for static pool allocator ID
        };

        //one stateful scalable dynamic pool allocator
        template<typename T>
        class ScalablePoolAllocator
        {
        private:
            MemoryPool::Scalable::ScalablePool* memory_pool_{ nullptr };
            const std::thread::id   owner_id_{ std::this_thread::get_id() };
        public:
            using value_type = T;
            template<typename U>
            struct rebind {
                using other = ScalablePoolAllocator<U>;
            };
        public:
            explicit ScalablePoolAllocator(MemoryPool::Scalable::ScalablePool* pool):memory_pool_(pool) {
                assert(pool != nullptr);
            }
            template<typename U>
            ScalablePoolAllocator(const ScalablePoolAllocator<U>& other) :memory_pool_(other.memory_pool_) {}
            template<typename U>
            ScalablePoolAllocator(ScalablePoolAllocator<U>&& other) : memory_pool_(std::exchange(other.memory_pool_, nullptr)) {}
            virtual ~ScalablePoolAllocator() = default;
            [[nodiscard]] T* allocate(size_type n) {
                return static_cast<T*>(memory_pool_->allocate(sizeof(T) * n));
            }
            void deallocate(T* ptr, size_type n) {
                if (owner_id_ == std::this_thread::get_id()) {
                    memory_pool_->deallocate(ptr, sizeof(T) * n);
                }
                else
                {
                    //consumer in another thread, delay deallcate
                    memory_pool_->deallocate_external(ptr, sizeof(T) * n);
                }
            }
            template<class U>
            friend bool operator==(const ScalablePoolAllocator& lhs, const ScalablePoolAllocator<U>& rhs) {
                return lhs.memory_pool_ == rhs.memory_pool_;
            }
            template<class U>
            friend bool operator!=(const ScalablePoolAllocator& lhs, const ScalablePoolAllocator<U>& rhs) {
                return !(lhs == rhs);
            }

        };

        //scalable pool allocator for every thread
        template<typename T, size_type id = 0u>
        requires(!std::is_void_v<T>)
        static decltype(auto) TLSScalablePoolAllocatorInstance()
        {
            //all class in one thread shared a unique memory pool
            thread_local ScalablePoolAllocator<T> allocator{ MemoryPool::Scalable::TLSScalablePoolInstance<id>() };
            return &allocator;
        }

        template<typename T, template <typename> typename Allocator>
        class TraceProxyAllocator 
        {
        public:
            using value_type = T;
            template<typename U>
            struct rebind {
                using other = TraceProxyAllocator<U, Allocator>;
            };
        public:
            TraceProxyAllocator(Allocator<T>& alloc) :alloc_(alloc) {

            }
            [[nodiscard]] T* allocate(size_type n) {
                ++alloc_mem_count_;
                alloc_mem_size_ += sizeof(T) * n;
                return alloc_.allocate(n);
            } 
            void deallocate(T* ptr, size_type n) {
                --alloc_mem_count_;
                alloc_mem_size_ -= sizeof(T) * n;
                alloc_.deallocate(ptr, n);
            }
            const size_type GetAllocMemSize() const {
                return alloc_mem_size_;
            }
            const size_type GetAllocMemCount() const {
                return alloc_mem_count_;
            }
            friend bool operator==(const TraceProxyAllocator& lhs, const TraceProxyAllocator& rhs) {
                return lhs.alloc_ == rhs.alloc_;
            }
            friend bool operator!=(const TraceProxyAllocator& lhs, const TraceProxyAllocator& rhs) {
                return !(lhs == rhs);
            }
        private:
            Allocator<T>&    alloc_;
            size_type    alloc_mem_size_{ 0u };
            size_type    alloc_mem_count_{ 0u };
        };

        template<typename T, template<typename>typename Allocator>
        class SynchronizedWrapAllocator : public Allocator<T>
        {
        public:
            using value_type = T;
            using Parent = Allocator<T>;
            template<typename U>
            struct rebind {
                using other = SynchronizedWrapAllocator<U, Allocator>;
            };
        public:
            [[nodiscard]] T* allocate(size_type n) {
                SpinLock lock(lock_);
                return Parent::allocate(n);
            }
            void deallocate(T* ptr, size_type n) {
                SpinLock lock(lock_);
                Parent::deallocate(ptr, n);
            }
            friend bool operator==(const SynchronizedWrapAllocator& lhs, const SynchronizedWrapAllocator& rhs) {
                return (static_cast<const Parent&>(lhs)) == (static_cast<const Parent&>(rhs));
            }
            friend bool operator!=(const SynchronizedWrapAllocator& lhs, const SynchronizedWrapAllocator& rhs) {
                return !(lhs == rhs);
            }
        private:
            std::atomic_bool    lock_{ false };
        };
    }
}
