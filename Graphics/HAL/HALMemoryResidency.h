#pragma once
#include "Utils/CommonUtils.h"
#include "Core/EngineGlobalParams.h"
#include "HALResources.h"

#ifdef DEBUG
#define RESIDENCY_MEMORY_DEBUG_MARGIN 16
#define RESIDENCY_PRINT_ALLOC_INFO
#else
#define RESIDENCY_MEMORY_DEBUG_MARGIN 0
#endif

namespace Shard::HAL
{
    /** whether prefer to alloca dedicated memory block */
    REGIST_PARAM_TYPE(BOOL, RESIDENCY_MULTI_THREAD, false);
    REGIST_PARAM_TYPE(BOOL, RESIDENCY_PREFER_DEDICATED, false);
    REGIST_PARAM_TYPE(UINT, RESIDENCY_PREFER_BLOCK_SIZE, 256 * 1024 * 1024); //copy from VMA
    REGIST_PARAM_TYPE(UINT, RESIDENCY_ALLOCATE_STRATEGY, EAllocateStrategy::eTLSF);
    REGIST_PARAM_TYPE(UINT, RESIDENCY_DEFRAG_MAX_BYTES, 1);
    REGIST_PARAM_TYPE(UINT, RESIDENCY_DEFRAG_MAX_ALLOCATIONS, 12);
    REGIST_PARAM_TYPE(UINT, RESIDENCY_DEFRAG_MAX_IGNORE_ALLOCATIONS, 16);
#ifdef RESIDENCY_MEMORY_DEFRAG
    REGIST_PARAM_TYPE(UINT, RESIDENCY_DEFRAG_STRATEGY, EHALDefragFlagBits::eFast);
#endif

    using HALAllocHandle = uintptr_t;
    using HALSizeType = uint64_t;

    namespace {
        class AllocationRequest;
    }

    enum EHALMemoryUsageBits : uint32_t
    {
        eUnkown     = 0x0,
        eCopySrc    = 0x1,
        eCopyDst    = 0x2,
        eShader     = 0x4, //SRV, UAV, RTV. etc
    };
    
    DECLARE_ENUM_BIT_OPS(EHALMemoryUsageBits);

    using HALMemoryUsage = uint32_t;

    enum EHALMemoryFlagBits : uint32_t
    {
        eDeviceLocal = 0x1,
        eHostVisible = 0x2, //system memory, uncached, GPU can access it
        eHostCached = 0x4, //system memory, cached, GPU reads snoop CPU cache
        eHostCoherent = 0x8,

        //according to 'efficient use of gpu memory in modern game'
        eDefault = eDeviceLocal,
        eUpload = eHostVisible | eHostCoherent, //fast for sequential write 
        eReadBack = eHostVisible | eHostCoherent | eHostCached, 
        //writes performed by the host to that memory go through PCI Express bus. The performance of these writes may be limited,
        //but it may be fine, especially on PCIe 4.0, as long as rules of using uncached and write-combined memory are followed - only sequential writes and no reads.
        eBar    = eDeviceLocal | eHostVisible | eHostCoherent, //good for resources written by CPU read by GPU, more about ReBar& SAM
     
        //allocation strategy
        eStrategyMinOffset   = 0x10,
        eStrategyMinTime    = 0x20,
        eStrategyMinOccupy  = 0x40,
        eStrategyMask       = eStrategyMinOffset | eStrategyMinTime | eStrategyMinOccupy,

        //dedicated flag bits
        eForceDedicated     = 0x100,
        eAllowMapped        = 0x200,
        //mapped when allocated
        ePersistentMapped   = 0x400,
        eMapMask            = eAllowMapped | ePersistentMapped,

        //if new allocation cannot placed in any blocks or create a new one
        eNeverAlloc         = 0x800,
        //check whether allocation in budget
        eWithInBudget       = 0x1000,
        //lazy allocated, optimize for mobile 
        eLazily             = 0x2000,

        /* for support MSAA alignment as D3D12_HEAP_DESC structure says:
        * -D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT   defined as 64KB.
        * -D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT   defined as 4MB
        */
        eTilingMSAA         = 0x4000, //maybe should force it to be dedicated
        /*vulkan buffer or linear-tiling image*/
        eTilingLinear       = 0x8000,
        /*vulkan optimal-tiling image */
        eTilingOptimal      = 0x10000,
        eTilingMask         = eTilingMSAA | eTilingLinear | eTilingOptimal,
    };

    EHALMemoryUsageBits(EHALMemoryFlagBits);
    using HALMemoryFlags = uint32_t;

    static FORCE_INLINE bool IsMemoryMapAble(HALMemoryFlags flags) {
        return flags & Utils::EnumToInteger(EHALMemoryFlagBits::eHostVisible);
    }

    enum
    {
        MAX_POOL_COUNT = 32u,
        /*small limit on maximum number of allocations */
        MEMORY_MAX_ALLOCATIONS = 4096u,
    };

    struct HALManagedMemoryBlockCreateInfo
    {
        //HALMemoryUsage  type_{ 0u };
        //HALMemoryFlags  flags_{ 0u };
        //HALSizeType prefered_size_;
    };

    struct HALManagedMemoryBlock;
    struct HALManagedMemory
    {
        OVERLOAD_OPERATOR_NEW(HALManagedMemory);
        HALManagedMemoryBlock* parent_{ nullptr };
        HALSizeType    offset_{ 0u };
        HALSizeType    alignment_{ 0u };
        HALSizeType    size_{ 0u }; //assume size be 2^x
        void*   user_data_{ nullptr };
    };

    FORCE_INLINE HALManagedMemoryBlock* GetParent(HALManagedMemory& memory) {
        return memory.parent_;
    }

    struct MappingHysteresis
    {
        enum {
            COUNTER_MIN_EXTRA_MAPPING = 7,
        };
        uint32_t GetExtraMapping() const { 
#ifdef RESIDECNY_MAPPING_HYSTERESIS
            return extra_mapping_;
#else
            return 0u;
#endif
        }
        bool PostMap() {
#ifdef RESIDECNY_MAPPING_HYSTERESIS
            if (!extra_mapping_) {
                ++major_counter_;
                if (major_counter_ >= COUNTER_MIN_EXTRA_MAPPING) {
                    extra_mapping_ = 1u;
                    major_counter_ = 0u;
                    minor_counter_ = 0u;
                    return true;
                }
            }
            else
            {
                PostMinorCounter();
            }
#else
            return false;
#endif
        }
        void PostUnMap() {
#ifdef RESIDECNY_MAPPING_HYSTERESIS
            if (!extra_mapping_) {
                ++major_counter_;
            }
            else
            {
                PostMinorCounter();
            }
#endif
        }
        void PostAlloc() {
#ifdef RESIDECNY_MAPPING_HYSTERESIS
            if (!!extra_mapping_) {
                ++major_counter_;
            }
            else
            {
                PostMinorCounter();
            }
#endif
        }
        bool PostDeAlloc() {
#ifdef RESIDECNY_MAPPING_HYSTERESIS
            if (!!extra_mapping_) {
                ++major_counter_;
                if (major_counter_ >= COUNTER_MIN_EXTRA_MAPPING && major_counter_ > minor_counter_ + 1) {
                    extra_mapping_ = 0u;
                    major_counter_ = 0u;
                    minor_counter_ = 0u;
                    return true;
                }
            }
            else
            {
                PostMinorCounter();
            }
#else
            return false;
#endif
        }

#ifdef RESIDECNY_MAPPING_HYSTERESIS
        uint32_t    minor_counter_{ 0u };
        uint32_t    major_counter_{ 0u };
        uint32_t    extra_mapping_{ 0u };//0 or 1
        void PostMinorCounter() {
            if (minor_counter_ < major_counter_) {
                ++minor_counter_;
            }
            else if (major_counter_ > 0u)
            {
                --major_counter_;
                --minor_counter_;
            }
        }
#endif
    };

    struct HALManagedMemoryBlock
    {
        OVERLOAD_OPERATOR_NEW(HALManagedMemoryBlock);
        void*   header_{ nullptr };
        HALSizeType size_{ 0u };
        uint32_t    map_count_{ 0u };
        MappingHysteresis   map_hysteresis_;
        void*   rhi_mem_{ nullptr };
        void*   mapped_{ nullptr };
    };

    struct HALDedicatedMemoryBlock
    {
        HALSizeType size_{ 0u };
        void*   rhi_mem_{ nullptr };
        void*   user_data_{ nullptr };

        //instrusive list node
        HALAllocation*  prev_{ nullptr };
        HALAllocation*  next_{ nullptr };
    };

    struct HALAllocationCreateInfo
    {
        enum
        {
            DEDICATE_FLAGS_BIT = 0x1,
        };
        HALMemoryUsage  type_{ 0u };
        HALMemoryFlags  flags_{ 0u };
        HALSizeType    alignment_{ 0u };
        HALSizeType    size_{ 0u }; //assume size be 2^x
        //rhi spec user data
        std::shared_ptr<void>   user_data_{ nullptr };

        FORCE_INLINE void MakeDedicated() { size_ &= DEDICATE_FLAGS_BIT; }
        FORCE_INLINE bool IsDedicated()const { return size_ & DEDICATE_FLAGS_BIT; }
        FORCE_INLINE HALSizeType Size()const { return size_ & ~DEDICATE_FLAGS_BIT; }
    };

    struct HALAllocation
    {
        OVERLOAD_OPERATOR_NEW(HALAllocation);
        union {
            struct
            {
                HALManagedMemory    mem_;
                HALSizeType offset_;
            }managed_mem_;
            HALDedicatedMemoryBlock dedicated_mem_;
        };
        uint32_t    is_dedicated_ : 1;
        uint32_t    is_map_allowed_ : 1;
        uint32_t    pool_index_ : 8;
        uint64_t    last_time_stamp_{ 0u };
        void*   mapped_{ nullptr };
        HALAllocation() {}
    };

    struct HALAliasAllocation : HALAllocation
    {
        OVERLOAD_OPERATOR_NEW(HALAliasAllocation);
        explicit HALAliasAllocation(HALAllocation& alloc) :HALAllocation(alloc) {}
        mutable uint32_t ref_count_{ 0u };
    };

    static FORCE_INLINE bool IsDedicated(const HALAllocation& memory) {
        return memory.is_dedicated_;
    }
    static FORCE_INLINE HALManagedMemoryBlock* GetBlock(HALAllocation& memory) {
        if (!memory.is_dedicated_) {
            return GetParent(memory.managed_mem_.mem_);
        }
        return nullptr;
    }
    static FORCE_INLINE HALDedicatedMemoryBlock* GetDedicated(HALAllocation& memory) {
        if (memory.is_dedicated_) {
            return &memory.dedicated_mem_;
        }
        return nullptr;
    }
    static FORCE_INLINE HALSizeType GetSize(HALAllocation& memory) {
        if (!memory.is_dedicated_) {
            return memory.dedicated_mem_.size_;
        }
        return memory.managed_mem_.mem_.size_;//todo
    }

    template<typename MemType>
    FORCE_INLINE bool IsMapped(const MemType& mem) { return mem.mapped_ != nullptr; }

    template<typename MemType>
    FORCE_INLINE void*& GetPrivateData(MemType& mem) { return mem.user_data_; }

    struct HALMemroyStatistics
    {
        //block count
        uint32_t    block_count_{ 0u };
        HALSizeType block_bytes_{ 0u };;
        uint32_t    allocation_count_{ 0u };
        HALSizeType allocation_bytes_{ 0u };
        auto& operator+=(const HALMemroyStatistics& other) {
            block_count_ += other.block_count_;
            block_bytes_ += other.block_bytes_;
            allocation_count_ += other.allocation_count_;
            allocation_bytes_ += other.allocation_bytes_;
            return *this;
        }
    };

    struct HALMemoryDetailStatistics
    {
        HALMemroyStatistics stat_{};

        HALSizeType    allocation_min_size_{ 0u };
        HALSizeType    allocation_max_size_{ 0u };

        //unused memory range
        HALSizeType    unused_range_count_{ 0u };
        HALSizeType    unused_range_min_size_{ 0u };
        HALSizeType    unused_range_max_size_{ 0u };

        auto& operator+=(const HALMemoryDetailStatistics& other) {
            stat_ += other.stat_;
            unused_range_count_ += other.unused_range_count_;
            allocation_min_size_ = std::min(allocation_min_size_, other.allocation_min_size_);
            allocation_max_size_ = std::max(allocation_max_size_, other.allocation_max_size_);
            unused_range_min_size_ = std::min(unused_range_min_size_, other.unused_range_min_size_);
            unused_range_max_size_ = std::max(unused_range_max_size_, other.unused_range_max_size_);
            return *this;
        }

    };

    struct HALMemoryTotalStatistics
    {
        HALMemoryDetailStatistics   heap_details_[MAX_POOL_COUNT];
        HALMemoryDetailStatistics   total_;
    };

    struct HALMemoryBudget
    {
        //statistics
        std::atomic<uint32_t>   block_count_{ 0u };
        std::atomic<HALSizeType>    block_bytes_{ 0u };;
        std::atomic<uint32_t>   allocation_count_{0u};
        std::atomic<HALSizeType>    allocation_bytes_{ 0u };

        /** region get from backend, so no need to sync*/
        HALSizeType    usage_{ std::numeric_limits<HALSizeType>::max() }; //used size in bytes
        HALSizeType    budget_{ std::numeric_limits<HALSizeType>::max() }; //available size in bytes
    };

    struct HALMemoryTotalBudget
    {
        HALMemoryBudget pool_budget_[MAX_POOL_COUNT];
        std::atomic<HALSizeType> num_ops_{ 0u };
        std::atomic<uint32_t>    alloc_count_{ 0u };
    };

    /*memory statistic ops*/
    static FORCE_INLINE HALMemoryDetailStatistics& GetHeapMemoryDetailStatistics(HALMemoryTotalStatistics& statistics, uint16_t pool_index) {
        return statistics.heap_details_[pool_index];
    }

    static FORCE_INLINE HALMemoryBudget& GetHeapMemoryBudget(HALMemoryTotalBudget& budgets, uint16_t pool_index) {
        return budgets.pool_budget_[pool_index];
    }

    static FORCE_INLINE void AddStatisticsAllocation(HALMemroyStatistics& stats, HALSizeType sz)
    {
        stats.allocation_bytes_ += sz;
        ++stats.allocation_count_;
    }
    static FORCE_INLINE void RemoveStatisticsAllocation(HALMemroyStatistics& stats, HALSizeType sz)
    {
        stats.allocation_bytes_ -= sz;
        --stats.allocation_count_;
    }
    static FORCE_INLINE void AddDetailedStatisticsUnusedRange(HALMemoryDetailStatistics& stats, HALSizeType sz)
    {
        stats.unused_range_count_++;
        stats.unused_range_min_size_ = std::min(stats.unused_range_min_size_, sz);
        stats.unused_range_max_size_ = std::max(stats.allocation_max_size_, sz);
    }

    static FORCE_INLINE void AddDetailedStatisticsAllocation(HALMemoryDetailStatistics& stats, HALSizeType sz)
    {
        stats.stat_.allocation_count_++;
        stats.stat_.allocation_bytes_ += sz;
        stats.allocation_min_size_ = std::min(stats.allocation_min_size_, sz);
        stats.allocation_max_size_ = std::max(stats.allocation_max_size_, sz);
    }

    enum class EAllocateStrategy
    {
        eTLSF,
        eLinear, //to implement
    };

    enum EHALBlockVectorCreateFlagBits
    {
        eIgnoreBufferImageGanularity = 0x1,
    };

    using HALBlockVectorCreateFlags = uint32_t;

    class HALMemoryResidencyManager;
    class HALManagedMemoryBlockVector 
    {
    public:
        OVERLOAD_OPERATOR_NEW(HALManagedMemoryBlockVector);
        friend class HALMemoryResidencyDefragExecutor;
        HALManagedMemoryBlockVector(HALMemoryResidencyManager* const manager, const HALBlockVectorCreateFlags flags, const uint8_t pool_index, const uint32_t min_block_count, const uint32_t max_block_count,
                                     const HALSizeType preferred_block_size, const float priority, EAllocateStrategy algoritm, bool multithread);
        void Alloc(HALSizeType size, HALAllocation*& memory);
        void PostAlloc(HALManagedMemoryBlock& block);
        void DeAlloc(HALAllocation* memory);
        void PostDeAlloc(HALManagedMemoryBlock& block);
        HALManagedMemoryBlock* GetBlock(uint32_t index) { return blocks_[index]; }
        bool CreateBlock(HALSizeType block_size, HALManagedMemoryBlock*& block);
        void RemoveBlock(HALManagedMemoryBlock* block);
        void* MapBlock(HALManagedMemoryBlock& block);
        void UnMapBlock(HALManagedMemoryBlock& block);
        uint32_t GetTotalBlockCount() const { return blocks_.size(); }
        HALSizeType CalcTotalBlockSize() const;
        HALSizeType CalcMaxBlockSize()const;
        void SetSortIncremental(bool value) { is_sort_incremental_ = value; }
        void IncrementalSortBlockByFreeSize();
        void SortBlocksByFreeSize();
        HALSizeType GetPreferredBlockSize() const { return preferred_block_size_; }
        float GetPriority() const { return priority_; }
        bool IsEmpty()const { return blocks_.empty(); }
        bool IsExplicitSize()const { return !!is_explicit_size_; }

        //statistics
        void AddStatistics(HALMemroyStatistics& stats) const;
        void AddDetailedStatistics(HALMemoryDetailStatistics& stats) const;
        ~HALManagedMemoryBlockVector();
    private:
        bool AllocPage(const HALAllocationCreateInfo& alloc_info, HALSizeType size, HALSizeType alignment, HALAllocation*& allocation);
        bool AllocFromBlock(HALManagedMemoryBlock& block, HALSizeType size, HALSizeType alignment, HALMemoryFlags flags, void* user_data, HALManagedMemory& memory);
        bool CommitAllocationRequest(AllocationRequest& request, HALManagedMemoryBlock& block, HALSizeType alignment, HALMemoryFlags flags, void* user_data, HALManagedMemory& memory);
    private:
        uint32_t    is_sort_incremental_ : 1;
        uint32_t    is_explicit_size_ : 1;
        HALSizeType preferred_block_size_{ 0u };

        const float priority_{ 0.5f };
        const HALBlockVectorCreateFlags flags_{};
        const uint8_t   pool_index_{ 0u };
        const uint32_t    min_block_count_{ 0u };
        const uint32_t    max_block_count_{ std::numeric_limits<uint32_t>::max() };
        const EAllocateStrategy strategy_{ EAllocateStrategy::eTLSF };
        //using HALBlockPtrAllocator = std::remove_pointer_t<decltype(g_hal_allocator)>::rebind<HALManagedMemoryBlock*>::other;
        SmallVector<HALManagedMemoryBlock*>   blocks_; //todo
        HALMemoryResidencyManager* const manager_{ nullptr };
        mutable std::atomic<std::uintptr_t> lock_{ 0ull };
    };

    struct HALAllocationIntrusiveLinkedListTraits
    {
        using ItemType = HALAllocation;
        static const ItemType* GetPrev(const ItemType* curr) {
            assert(curr != nullptr && curr->is_dedicated_);
            return curr->dedicated_mem_.prev_;
        }
        static const ItemType* GetNext(const ItemType* curr) {
            assert(curr != nullptr && curr->is_dedicated_);
            return curr->dedicated_mem_.next_;
        }
        static ItemType*& AccessPrev(ItemType* curr) {
            assert(curr != nullptr && curr->is_dedicated_);
            return curr->dedicated_mem_.prev_;
        }
        static ItemType*& AccessNext(ItemType* curr) {
            assert(curr != nullptr && curr->is_dedicated_);
            return curr->dedicated_mem_.next_;
        }
    };

    class HALDedicatedMemoryBlockList
    {
    public:
        OVERLOAD_OPERATOR_NEW(HALDedicatedMemoryBlockList);
        friend class HALMemoryResidencyDefragExecutor;
        HALDedicatedMemoryBlockList(HALMemoryResidencyManager* const manager, const uint8_t pool_index, const float priority, bool multithread);
        void Alloc(HALSizeType size, HALAllocation*& memory);
        void DeAlloc(HALAllocation* memory);
        void SortBySize();
        float GetPriority() const { return priority_; }
        bool IsEmpty()const { return free_blocks_.empty(); }
        //statistics
        void AddStatistics(HALMemroyStatistics& stats) const;
        void AddDetailedStatistics(HALMemoryDetailStatistics& stats) const;
        ~HALDedicatedMemoryBlockList();
    private:
        using HALAllocationList = Utils::IntrusiveLinkedList<HALAllocationIntrusiveLinkedListTraits>;
        const uint8_t   pool_index_{ 0u };
        const float priority_{ 1.f };
        HALAllocationList  free_blocks_; 
        HALMemoryResidencyManager* const    manager_{ nullptr };
        mutable std::atomic<std::uintptr_t>    lock_{ 0ull };
    };

    class MINIT_API HALMemoryResidencyManager
    {
    public:
        friend class HALMemoryResidencyDefragExecutor;
        friend class HALManagedMemoryBlockVector;
        friend class HALDedicatedMemoryBlockList;

        using Ptr = HALMemoryResidencyManager*;

        HALMemoryResidencyManager() = default;
        
        /**
         * \brief init manager with platform special entity
         * \param rhi_entity backend special entity
         */
        virtual void Init(HALGlobalEntity* rhi_entity) = 0;
        /**
         * \brief get resource memory information from rhi
         * \param res_ptr
         * \return
         */
        virtual HALAllocationCreateInfo GetBufferResidencyInfo(HALBuffer* res_ptr) const = 0;
        virtual HALAllocationCreateInfo GetTextureResidencyInfo(HALTexture* res_ptr) const = 0;
        virtual bool IsUMA() const { return false;  }
        virtual bool IsMapInResourceLevel() const { return true; }
        /*for create_info has platform special user_data, so must implement this function each platform respectively*/
        bool IsManagedMemoryCreateInfoSupported(const HALAllocationCreateInfo& create_info) const { return FindMemoryPoolIndex(create_info.type_, create_info.flags_) != std::numeric_limits<uint8_t>::max(); }
        /**
         * \brief alloc a memory block according to mem_info
         * \param memory allocation information
         * \return memory 
         */
        [[nodiscard]]HALAllocation* AllocMemory(const HALAllocationCreateInfo& mem_info);
        /**
         * \brief create memory batch of same mem_info
         * \param mem_info
         * \param count allocation count need to allocate
         * \return 
         */
        [[nodiscard]]HALAllocation* AllocMemoryBatch(const HALAllocationCreateInfo& mem_info, uint32_t count);
        /**
         * \brief allocate dedicate memory
         * \return dedicated memory
         */
        [[nodiscard]]HALAllocation* AllocDedicatedMemory(const HALAllocationCreateInfo& mem_info);
        /**
         * \brief allocate one memory from prev alloc block,if block is nullptr, alloc one
         * \param mem_info
         * \return managed memory
         */
        [[nodiscard]]HALAllocation* AllocManagedMemory(const HALAllocationCreateInfo& mem_info);
        /**
         * \brief dellocate allocation
         */
        void DeAllocMemory(HALAllocation* allocation);
        /*
        * \brief return true if multithread supported inner
        */
        bool IsMultithreadSupported() const { return global_lock_.load() != 0u; }
        void SetBlockVectorSortIncremental(bool incremental);
        /**
         * \brief turn on budget limit for pool with limit size
         * \param pool_index: pool to turn on budget limit
         * \param limit_size: pool limit size
         */
        void TurnOnPoolBudgetLimit(uint8_t pool_index, HALSizeType limit_size);
        /**
        * \brief turn off budget limit for pool
        */
        void TurnOffPoolBudgetLimit(uint8_t pool_index);
        void DeAlloc(HALAllocation* allcation);
#if RESIDENCY_MEMORY_DEBUG_MARGIN
        void WriteMagicValueToAllocation(HALAllocation& allocation, HALSizeType offset);
        bool VerifyAllocationMagicValue(HALAllocation& allocation, HALSizeType offset);
#endif
        void GetAllocationInfo(HALAllocation& allocation, HALAllocationCreateInfo& info);
        uint32_t GetMemoryPoolCount()const { return managed_vec_.size(); }
        
        void MakeBufferResident(HALBuffer* buffer_ptr, HALAllocation& allocation, HALSizeType offset=0u);
        void MakeTextureResident(HALTexture* texture_ptr, HALAllocation& allocation, HALSizeType offset=0u);
        /**
         *\brief unbind a resource
         * \param res_ptr
         */
        virtual void Evict(HALResource* res_ptr) = 0;
        /**
         * \brief map resource to cpu 
         * \param res_ptr:resource pointer to map, allocation: allocation pointer
         * \return 
         */
        virtual void* Map([[maybe_unused]] HALResource* res_ptr, [[maybe_unused]] HALAllocation* allocation) = 0;
        /**
         * \brief unmap resource 
         * \param res_ptr:resource pointer to map, allocationL allocation pointer 
         */
        virtual void UnMap([[maybe_unused]] HALResource* res_ptr, [[maybe_unused]] HALAllocation* allocation) = 0;
        /**
         * \brief generate platform spec information by gusss
         * \param create_info
         */
        virtual void GetAllocationBackendSpecInfoApprox(HALAllocationCreateInfo& create_info) = 0;
#ifdef RESIDENCY_PRINT_ALLOC_INFO
        virtual void OutputMemoryAllocationInfos()const;
#endif
        virtual ~HALMemoryResidencyManager() = default;

        void Tick(float time);
    private:
        DISALLOW_COPY_AND_ASSIGN(HALMemoryResidencyManager);
    protected:
        //void CommitAllocationRequst(void* private_data, HALManagedMemory** managed_memory);
        HALMemoryBudget& GetMemoryBudget(uint8_t pool_index);
        HALMemoryTotalBudget& GetTotalMemoryBudget();
        HALMemoryDetailStatistics& GetMemoryStatistics(uint8_t pool_index);
        HALMemoryTotalStatistics& GetTotalMemoryStatistics();
        void CalcMemoryStatistics(uint8_t pool_index);
        void CalcTotalMemoryStatistics();
        bool AccquireMemoryBudgetAllocation(uint8_t pool_index, HALSizeType size);
        bool AccquireMemoryBudgetBlock(uint8_t pool_index, HALSizeType size);
        void ReleaseMemoryBudgetAllocation(uint8_t pool_index, HALSizeType size);
        void ReleaseMemoryBudgetBlock(uint8_t pool_index, HALSizeType size);
        virtual uint32_t GetMaxAllocationCount() const { return std::numeric_limits<uint32_t>::max(); }
        virtual uint8_t FindMemoryPoolIndex(HALMemoryUsage usage, HALMemoryFlags flags, void* user_data = nullptr) const = 0;
        virtual HALSizeType GetMemoryPoolPreferredSize(uint8_t pool_index) = 0;
        virtual void UpdateMemoryBudget() { mem_budget_.num_ops_.store(0u); };

        virtual void MakeBufferResidentImpl(HALBuffer* res_ptr, void* rhi_mem, HALSizeType offset) = 0;
        virtual void MakeTextureResidentImpl(HALTexture* res_ptr, void* rhi_mem, HALSizeType offset) = 0;
        virtual [[nodiscard]]void* MallocRawMemory(uint32_t pool_index, uint64_t size, float priority, [[maybe_unused]] void* plat_data = nullptr, [[maybe_unused]] void* user_data = nullptr) = 0;
        virtual void FreeRawMemory(void* memory, uint32_t pool_index, HALSizeType offset, HALSizeType size, [[maybe_unused]] void*& mapped) = 0;
        virtual void MapRawMemory(void* memory, HALSizeType offset, HALSizeType size, uint32_t flags, [[maybe_unused]] void*& mapped) {};
        virtual void UnMapRawMemory(void* memory) {};
    protected:
        mutable std::atomic<std::uintptr_t> global_lock_{ 0ull };
        //bitset of budget limits enable mask
        uint32_t budget_limit_mask_{ 0u };
        Array<HALSizeType, MAX_POOL_COUNT>  pool_budget_limit_;
        Array<HALManagedMemoryBlockVector*, MAX_POOL_COUNT>  managed_vec_; //todo d3d12 committed 
        Array<HALDedicatedMemoryBlockList*, MAX_POOL_COUNT>  dedicated_vec_; //todo d3d12 placed
        uint64_t curr_time_stamp_;
        uint64_t curr_frame_index_;
        HALMemoryTotalBudget mem_budget_;
        HALMemoryTotalStatistics mem_statistics_;
    };

#ifdef RESIDENCY_MEMORY_DEFRAG

    struct HALDefragStatistics
    {
        HALSizeType moved_bytes_{ 0u };
        HALSizeType freed_bytes_{ 0u };
        uint32_t    freed_blocks_{ 0u };
        uint32_t    moved_allocations_{ 0u };
        auto& operator+=(const HALDefragStatistics& other) {
            moved_bytes_ += other.moved_bytes_;
            freed_bytes_ += other.freed_bytes_;
            freed_blocks_ += other.freed_blocks_;
            moved_allocations_ += other.moved_allocations_;
            return *this;
        }
    };

    enum EHALDefragFlagBits : uint32_t
    {
        eFast       = 0x1,
        eBalanced   = 0x2,
        eFull       = 0x4,
        eExtensive  = 0x8,
        eNeedState  = eBalanced|eExtensive,
        eMask       = eFast|eBalanced|eFull|eExtensive,
    };

    using HALDefragFlags = uint32_t;

    enum class EMoveOp
    {
        eCopy       = 0x1,
        eIgnore     = 0x2,
        eDestroy    = 0x4,
        /*ext bits for defrag test status*/
        ePass       = 0x8,
        eEnd        = 0x10,
        eStatusMask = ePass | eEnd,
    };

    struct DefragMove
    {
        EMoveOp op_{ EMoveOp::eCopy };
        HALAllocation* src_{ nullptr };
        HALAllocation* tmp_dst_{ nullptr };
    };

    struct DefragMoveBatchInfo
    {
        uint32_t count_{ 0u };
        DefragMove* moves_{ nullptr };
    };

    class MINIT_API HALMemoryResidencyDefragExecutor final
    {
    public:
        HALMemoryResidencyDefragExecutor(HALMemoryResidencyManager* manager);
        ~HALMemoryResidencyDefragExecutor();
        void BeginDefragPass(HALMemoryResidencyManager& manager, DefragMoveBatchInfo& moves);
        void EndDefragPass(HALMemoryResidencyManager& manager, DefragMoveBatchInfo& move_info);
        HALDefragStatistics GetDefragPassStatistics()const { return global_stats_; }
        EMoveOp TestMoveOp(HALSizeType size) const;
        bool UpdatePassStatus(HALSizeType move_size);
        auto& GetMoves() { return moves_; }
        HALSizeType GetMovesCount() const { return moves_.size(); }
        auto* GetDefragState() { return (flags_ & EHALDefragFlagBits::eNeedState) ? defrag_state_.get() : nullptr; }
    private:
        DISALLOW_COPY_AND_ASSIGN(HALMemoryResidencyDefragExecutor);
        using DefragFunc = std::function<bool(HALMemoryResidencyDefragExecutor&, HALManagedMemoryBlockVector& vec, uint32_t index, bool update)>;
    private:
        HALDefragFlags  flags_;
        HALMemoryResidencyManager* const residency_manager_{ nullptr };
        HALSizeType pass_max_bytes_;
        HALSizeType pass_max_allocations_;
        HALDefragStatistics global_stats_;
        HALDefragStatistics pass_stats_;
        DefragFunc  defrag_func_;
        uint8_t max_ignore_allocs_count_;
        uint8_t ignore_allocs_count_{ 0u };
        uint32_t    immoveable_block_count_{ 0u };
        std::shared_ptr<void>   defrag_state_; 
        using DefragMoveAllocator = REBIND_ALLOCATOR(std::remove_pointer_t<decltype(g_hal_allocator)>, DefragMove);
        Vector<DefragMove, DefragMoveAllocator>  moves_{ *g_hal_allocator };
    };
#endif

}

/**
 * todo "DX12 Memory Management in Snowdrop on PC"
 */





