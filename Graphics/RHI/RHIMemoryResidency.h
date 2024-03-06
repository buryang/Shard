#pragma once
#include "Utils/CommonUtils.h"
#include "Core/EngineGlobalParams.h"
#include "RHIResources.h"

#ifdef DEBUG
#define RESIDENCY_RESIDENCY_MEMORY_DEBUG_MARGIN 16
#else
#define RESIDENCY_MEMORY_DEBUG_MARGIN 0
#endif

namespace Shard::RHI
{
    /** whether prefer to alloca dedicated memory block */
    REGIST_PARAM_TYPE(BOOL, RESIDENCY_MULTI_THREAD, false);
    REGIST_PARAM_TYPE(BOOL, RESIDENCY_PREFER_DEDICATED, false);
    REGIST_PARAM_TYPE(UINT, RESIDENCY_PREFER_BLOCK_SIZE, 256 * 1024 * 1024); //copy from VMA
    REGIST_PARAM_TYPE(UINT, RESIDENCY_ALLOCATE_STRATEGY, EAllocateStrategy::eTLSF);
    REGIST_PARAM_TYPE(UINT, RESIDENCY_DEFRAG_STRATEGY, ERHIDefragFlagBits::eFast);
    REGIST_PARAM_TYPE(UINT, RESIDENCY_DEFRAG_MAX_BYTES, 1);
    REGIST_PARAM_TYPE(UINT, RESIDENCY_DEFRAG_MAX_ALLOCATIONS, 12);
    REGIST_PARAM_TYPE(UINT, RESIDENCY_DEFRAG_MAX_IGNORE_ALLOCATIONS, 16);

    using RHIAllocHandle = uintptr_t;
    using RHISizeType = uint64_t;

    namespace {
        class AllocationRequest;
    }

    enum ERHIMemoryUsageBits : uint32_t
    {
        eUnkown     = 0x0,
        eCopySrc    = 0x1,
        eCopyDst    = 0x2,
        eSRV        = 0x4,
        eUAV        = 0x8,
        eRTV        = 0x10,
    };

    using RHIMemoryUsage = uint32_t;

    enum ERHIMemoryFlagBits : uint32_t
    {
        eDeviceLocal    = 0x1,
        eHostVisible    = 0x2, //system memory, uncached, GPU can access it
        eHostCached     = 0x4, //system memory, cached, GPU reads snoop CPU cache
        eHostCoherent   = 0x8,

        //allocation strategy
        eStrategyMinOffset   = 0x10,
        eStrategyMinTime    = 0x20,
        eStrategyMinOccupy  = 0x40,
        eStrategyMask       = eStrategyMinOffset | eStrategyMinTime | eStrategyMinOccupy,

        //dedicated flag bits
        eForceDedicated     = 0x100,
        eAllowMapped        = 0x200,
        //mapped when allocated
        ePersistentMapped   = 0x400 | eAllowMapped,
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

    using RHIMemoryFlags = uint32_t;

    enum
    {
        MAX_POOL_COUNT = 32u,
        /*small limit on maximum number of allocations */
        MEMORY_MAX_ALLOCATIONS = 4096u,
    };

    struct RHIManagedMemoryBlockCreateInfo
    {
        //RHIMemoryUsage  type_{ 0u };
        //RHIMemoryFlags  flags_{ 0u };
        //RHISizeType prefered_size_;
    };

    struct RHIManagedMemoryBlock;
    struct RHIManagedMemory
    {
        OVERLOAD_OPERATOR_NEW(RHIManagedMemory);
        RHIManagedMemoryBlock* parent_{ nullptr };
        RHISizeType    offset_{ 0u };
        RHISizeType    alignment_{ 0u };
        RHISizeType    size_{ 0u }; //assume size be 2^x
        void*   user_data_{ nullptr };
    };

    FORCE_INLINE RHIManagedMemoryBlock* GetBlock(RHIManagedMemory& memory) {
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

    struct RHIManagedMemoryBlock
    {
        OVERLOAD_OPERATOR_NEW(RHIManagedMemoryBlock);
        void*   header_{ nullptr };
        RHISizeType size_{ 0u };
        uint32_t    map_count_{ 0u };
        MappingHysteresis   map_hysteresis_;
        void*   rhi_mem_{ nullptr };
        void*   mapped_{ nullptr };
    };

    struct RHIDedicatedMemoryBlock
    {
        OVERLOAD_OPERATOR_NEW(RHIDedicatedMemoryBlock);
        RHISizeType size_{ 0u };
        void*   rhi_mem_{ nullptr };
        void*   user_data_{ nullptr };
    };

    struct RHIAllocationCreateInfo
    {
        enum
        {
            DEDICATE_FLAGS_BIT = 0x1,
        };
        RHIMemoryUsage  type_{ 0u };
        RHIMemoryFlags  flags_{ 0u };
        RHISizeType    alignment_{ 0u };
        RHISizeType    size_{ 0u }; //assume size be 2^x
        //rhi spec user data
        std::shared_ptr<void>   user_data_{ nullptr };

        FORCE_INLINE void MakeDedicated() { size_ &= DEDICATE_FLAGS_BIT; }
        FORCE_INLINE bool IsDedicated()const { return size_ & DEDICATE_FLAGS_BIT; }
        FORCE_INLINE RHISizeType Size()const { return size_ & ~DEDICATE_FLAGS_BIT; }
    };

    struct RHIAllocation
    {
        OVERLOAD_OPERATOR_NEW(RHIAllocation);
        union {
            struct
            {
                RHIManagedMemory    mem_;
                RHISizeType offset_;
            }managed_mem_;
            RHIDedicatedMemoryBlock dedicated_mem_;
        };
        uint32_t    is_dedicated_ : 1;
        uint32_t    is_map_allowed_ : 1;
        uint32_t    pool_index_ : 8;
        uint64_t    last_time_stamp_{ 0u };
        void*   mapped_{ nullptr };
        RHIAllocation() {}
    };

    FORCE_INLINE bool IsDedicated(const RHIAllocation& memory) {
        return memory.is_dedicated_;
    }
    FORCE_INLINE RHIManagedMemoryBlock* GetBlock(RHIAllocation& memory) {
        if (!memory.is_dedicated_) {
            return GetBlock(memory.managed_mem_.mem_);
        }
        return nullptr;
    }
    FORCE_INLINE RHIDedicatedMemoryBlock* GetDedicated(RHIAllocation& memory) {
        if (memory.is_dedicated_) {
            return &memory.dedicated_mem_;
        }
        return nullptr;
    }
    FORCE_INLINE RHISizeType GetSize(RHIAllocation& memory) {
        if (!memory.is_dedicated_) {
            return memory.dedicated_mem_.size_;
        }
        return memory.managed_mem_.mem_.size_;//todo
    }

    template<typename MemType>
    FORCE_INLINE bool IsMapped(const MemType& mem) { return mem.mapped_ != nullptr; }

    template<typename MemType>
    FORCE_INLINE void*& GetPrivateData(MemType& mem) { return mem.user_data_; }

    struct RHIMemroyStatistics
    {
        //block count
        uint32_t    block_count_{ 0u };
        RHISizeType block_bytes_{ 0u };;
        uint32_t    allocation_count_{ 0u };;
        uint32_t    allocation_bytes_{ 0u };;
        auto& operator+=(const RHIMemroyStatistics& other) {
            block_count_ += other.block_count_;
            block_bytes_ += other.block_bytes_;
            allocation_count_ += other.allocation_count_;
            allocation_bytes_ += other.allocation_bytes_;
            return *this;
        }
    };

    struct RHIMemoryDetailStatistics
    {
        RHIMemroyStatistics stat_{};

        RHISizeType    allocation_min_size_{ 0u };
        RHISizeType    allocation_max_size_{ 0u };

        //unused memory range
        RHISizeType    unused_range_count_{ 0u };
        RHISizeType    unused_range_min_size_{ 0u };
        RHISizeType    unused_range_max_size_{ 0u };

        auto& operator+=(const RHIMemoryDetailStatistics& other) {
            stat_ += other.stat_;
            unused_range_count_ += other.unused_range_count_;
            allocation_min_size_ = std::min(allocation_min_size_, other.allocation_min_size_);
            allocation_max_size_ = std::max(allocation_max_size_, other.allocation_max_size_);
            unused_range_min_size_ = std::min(unused_range_min_size_, other.unused_range_min_size_);
            unused_range_max_size_ = std::max(unused_range_max_size_, other.unused_range_max_size_);
            return *this;
        }

    };

    struct RHIMemoryTotalStatistics
    {
        RHIMemoryDetailStatistics   heap_details_[MAX_POOL_COUNT];
        RHIMemoryDetailStatistics   total_;
        void* platform_spec_{ nullptr };
    };

    struct RHIMemoryBudget
    {
        RHIMemroyStatistics stat_;

        /** region get from backend*/
        RHISizeType    usage_; //used size in bytes
        RHISizeType    budget_; //available size in bytes
    };

    struct RHIMemoryTotalBudget
    {
        RHIMemoryBudget pool_budget_[MAX_POOL_COUNT];
        RHISizeType num_ops_{ 0u };
        uint32_t    alloc_count_{ 0u };
    };

    static FORCE_INLINE RHIMemoryDetailStatistics& GetHeapMemoryDetailStatistics(RHIMemoryTotalStatistics& statistics, uint16_t pool_index) {
        return statistics.heap_details_[pool_index];
    }

    static FORCE_INLINE RHIMemoryBudget& GetHeapMemoryBudget(RHIMemoryTotalBudget& budgets, uint16_t pool_index) {
        return budgets.pool_budget_[pool_index];
    }

    enum class EAllocateStrategy
    {
        eTLSF,
        eLinear, //to implement
    };

    enum ERHIBlockVectorCreateFlagBits
    {
        eIgnoreBufferImageGanularity    = 0x1,
    };
    using RHIBlockVectorCreateFlags = uint32_t;

    class RHIMemoryResidencyManager;
    class RHIManagedMemoryBlockVector 
    {
    public:
        OVERLOAD_OPERATOR_NEW(RHIManagedMemoryBlockVector);
        friend class RHIMemoryResidencyDefragExecutor;
        RHIManagedMemoryBlockVector(RHIMemoryResidencyManager* const manager, const RHIBlockVectorCreateFlags flags, const uint8_t pool_index, const uint32_t min_block_count, const uint32_t max_block_count,
                                     const RHISizeType preferred_block_size, const float priority, EAllocateStrategy algoritm, bool multithread);
        void Alloc(RHISizeType size, RHIAllocation*& memory);
        void PostAlloc(RHIManagedMemoryBlock& block);
        void DeAlloc(RHIAllocation* memory);
        void PostDeAlloc(RHIManagedMemoryBlock& block);
        RHIManagedMemoryBlock* GetBlock(uint32_t index) { return blocks_[index]; }
        bool CreateBlock(RHISizeType block_size, RHIManagedMemoryBlock*& block);
        void RemoveBlock(RHIManagedMemoryBlock* block);
        void* MapBlock(RHIManagedMemoryBlock& block);
        void UnMapBlock(RHIManagedMemoryBlock& block);
        uint32_t GetTotalBlockCount() const { return blocks_.size(); }
        RHISizeType CalcTotalBlockSize() const;
        RHISizeType CalcMaxBlockSize()const;
        void SetSortIncremental(bool value) { is_sort_incremental_ = value; }
        void IncrementalSortBlockByFreeSize();
        void SortBlocksByFreeSize();
        RHISizeType GetPreferredBlockSize() const { return preferred_block_size_; }
        float GetPriority() const { return priority_; }
        bool IsEmpty()const { return blocks_.empty(); }
        bool IsExplicitSize()const { return !!is_explicit_size_; }

        //statistics
        void AddStatistics(RHIMemroyStatistics& stats) const;
        void AddDetailedStatistics(RHIMemoryDetailStatistics& stats) const;
        ~RHIManagedMemoryBlockVector();
    private:
        bool AllocPage(const RHIAllocationCreateInfo& alloc_info, RHISizeType size, RHISizeType alignment, RHIAllocation*& allocation);
        bool AllocFromBlock(RHIManagedMemoryBlock& block, RHISizeType size, RHISizeType alignment, RHIMemoryFlags flags, void* user_data, RHIManagedMemory& memory);
        bool CommitAllocationRequest(AllocationRequest& request, RHIManagedMemoryBlock& block, RHISizeType alignment, RHIMemoryFlags flags, void* user_data, RHIManagedMemory& memory);
    private:
        uint32_t    is_sort_incremental_ : 1;
        uint32_t    is_explicit_size_ : 1;
        RHISizeType preferred_block_size_{ 0u };

        const float priority_{ 0.5f };
        const RHIBlockVectorCreateFlags flags_{};
        const uint8_t   pool_index_{ 0u };
        const uint32_t    min_block_count_{ 0u };
        const uint32_t    max_block_count_{ std::numeric_limits<uint32_t>::max() };
        const EAllocateStrategy strategy_{ EAllocateStrategy::eTLSF };
        SmallVector<RHIManagedMemoryBlock*> blocks_; //todo
        RHIMemoryResidencyManager* const manager_{ nullptr };
        mutable std::atomic<std::uintptr_t> lock_{ 0ull };
    };

    class RHIDedicatedMemoryBlockList
    {
    public:
        OVERLOAD_OPERATOR_NEW(RHIDedicatedMemoryBlockList);
        friend class RHIMemoryResidencyDefragExecutor;
        RHIDedicatedMemoryBlockList(RHIMemoryResidencyManager* const manager, const uint8_t pool_index, const float priority, bool multithread);
        void Alloc(RHISizeType size, RHIAllocation*& memory);
        void DeAlloc(RHIAllocation*& memory);
        void* MapBlock(RHIDedicatedMemoryBlock& block);
        void UnMapBlock(RHIDedicatedMemoryBlock& block);
        void SortBySize();
        float GetPriority() const { return priority_; }
        bool IsEmpty()const { return free_blocks_.empty(); }
        //statistics
        void AddStatistics(RHIMemroyStatistics& stats) const;
        void AddDetailedStatistics(RHIMemoryDetailStatistics& stats) const;
        ~RHIDedicatedMemoryBlockList();
    private:
        const uint8_t   pool_index_{ 0u };
        const float priority_{ 1.f };
        List<RHIAllocation*>  free_blocks_; //todo
        RHIMemoryResidencyManager* const    manager_{ nullptr };
        mutable std::atomic<std::uintptr_t>    lock_{ 0ull };
    };

    class MINIT_API RHIMemoryResidencyManager
    {
    public:
        friend class RHIMemoryResidencyDefragExecutor;
        friend class RHIManagedMemoryBlockVector;
        friend class RHIDedicatedMemoryBlockList;

        using Ptr = RHIMemoryResidencyManager*;

        RHIMemoryResidencyManager() = default;
        
        /**
         * \brief init manager with platform special entity
         * \param rhi_entity backend special entity
         */
        virtual void Init(RHIGlobalEntity::Ptr rhi_entity) = 0;
        /**
         * \brief get resource memory information from rhi
         * \param res_ptr
         * \return 
         */
        virtual RHIAllocationCreateInfo GetResourceResidencyInfo(RHIResource::Ptr res_ptr) const = 0;
        virtual bool IsUMA() const { return false;  }
        virtual bool IsMapInResourceLevel() const { return true; }
        /*for create_info has platform special user_data, so must implement this function each platform respectively*/
        bool IsManagedMemoryCreateInfoSupported(const RHIAllocationCreateInfo& create_info) const { return FindMemoryPoolIndex(create_info.type_, create_info.flags_) != std::numeric_limits<uint8_t>::max(); }
        /**
         * \brief alloc a memory block according to mem_info
         * \param memory allocation information
         * \return memory 
         */
        [[nodiscard]]RHIAllocation* AllocMemory(const RHIAllocationCreateInfo& mem_info);
        /**
         * \brief create memory batch of same mem_info
         * \param mem_info
         * \param count allocation count need to allocate
         * \return 
         */
        [[nodiscard]]RHIAllocation* AllocMemoryBatch(const RHIAllocationCreateInfo& mem_info, uint32_t count);
        /**
         * \brief allocate dedicate memory
         * \return dedicated memory
         */
        [[nodiscard]]RHIAllocation* AllocDedicatedMemory(const RHIAllocationCreateInfo& mem_info);
        /**
         * \brief allocate one memory from prev alloc block,if block is nullptr, alloc one
         * \param mem_info
         * \return managed memory
         */
        [[nodiscard]]RHIAllocation* AllocManagedMemory(const RHIAllocationCreateInfo& mem_info);
        /*
        * \brief return true if multithread supported inner
        */
        bool IsMultithreadSupported() const { return global_lock_.load() != 0u; }
        void SetBlockVectorSortIncremental(bool incremental);

        void DeAlloc(RHIAllocation* allcation);
#ifdef DEBUG
        void WriteMagicValueToAllocation(RHIAllocation& allocation, RHISizeType offset);
        bool VerifyAllocationMagicValue(RHIAllocation& allocation) const;
#endif
        uint32_t GetMemoryPoolCount()const { return managed_vec_.size(); }
        virtual void MakeResident(RHIResource::Ptr res_ptr, RHIAllocation& allocation) = 0;
        /**
         * \brief make a resource resident on managed memory
         * \param res_ptr
         * \param managed_mem
         */
        virtual void MakeResident(RHIResource::Ptr res_ptr, RHIManagedMemory& managed_mem, RHISizeType offset) = 0;
        /**
         * \breif bind resource to a dedicated memory 
         * \param res_ptr
         * \param dedicated_mem memory binded to
         */
        virtual void MakeResident(RHIResource::Ptr res_ptr, RHIDedicatedMemoryBlock& dedicated_mem) = 0;
        /**
         * \bind resource atomatically
         * \param res_ptr
         */
        virtual void MakeResident(RHIResource::Ptr res_ptr);
        /**
         *\brief unbind a resource
         * \param res_ptr
         */
        virtual void Evict(RHIResource::Ptr res_ptr) = 0;
        /**
         * \brief map resource to cpu 
         * \param res_ptr:resource pointer to map, allocation: allocation pointer
         * \return 
         */
        virtual void* Map([[maybe_unused]] RHIResource::Ptr res_ptr, [[maybe_unused]] RHIAllocation* allocation) = 0;
        /**
         * \brief unmap resource 
         * \param res_ptr:resource pointer to map, allocationL allocation pointer 
         */
        virtual void UnMap([[maybe_unused]] RHIResource::Ptr res_ptr, [[maybe_unused]] RHIAllocation* allocation) = 0;
        /**
         * \brief generate platform spec information by gusss
         * \param create_info
         */
        virtual void GetAllocationBackendSpecInfoApprox(RHIAllocationCreateInfo& create_info) = 0;
        virtual ~RHIMemoryResidencyManager() = default;

        void Tick(float time);
    private:
        DISALLOW_COPY_AND_ASSIGN(RHIMemoryResidencyManager);
    protected:
        //void CommitAllocationRequst(void* private_data, RHIManagedMemory** managed_memory);
        RHIMemoryBudget& GetMemoryBudget(uint8_t pool_index);
        RHIMemoryDetailStatistics& GetMemoryStatistics(uint8_t pool_index);
        virtual uint32_t GetMaxAllocationCount() const { return std::numeric_limits<uint32_t>::max(); }
        virtual void CalcMemoryStatistics(RHIMemoryDetailStatistics& statistic, uint32_t pool_index) const = 0;
        virtual uint8_t FindMemoryPoolIndex(RHIMemoryUsage usage, RHIMemoryFlags flags, void* user_data = nullptr) const = 0;
        virtual RHISizeType GetMemoryPoolPreferredSize(uint8_t pool_index) = 0;
        virtual void UpdateMemoryBudget() { mem_budget_.num_ops_ = 0u; };
        virtual void* MallocRawMemory(uint32_t pool_index, uint64_t size, float priority, [[maybe_unused]] void* plat_data = nullptr, [[maybe_unused]] void* user_data = nullptr) = 0;
        /*raw rhi memory operation should implement*/
        virtual void FreeRawMemory(void* memory, RHISizeType offset, RHISizeType size, [[maybe_unused]] void*& mapped) = 0;
        virtual void MapRawMemory(void* memory, RHISizeType offset, RHISizeType size, uint32_t flags, [[maybe_unused]] void*& mapped) {};
        virtual void UnMapRawMemory(void* memory) {};
    protected:
        std::atomic<std::uintptr_t> global_lock_{ 0ull };
        Array<RHIManagedMemoryBlockVector*, MAX_POOL_COUNT>  managed_vec_; //todo d3d12 committed 
        Array<RHIDedicatedMemoryBlockList*, MAX_POOL_COUNT>  dedicated_vec_; //todo d3d12 placed
        uint64_t curr_time_stamp_;
        uint64_t curr_frame_index_;
        RHIMemoryTotalBudget mem_budget_;
        RHIMemoryTotalStatistics mem_statistics_;
    };

#if 1//def USE_MEMORY_DEFRAG

    struct RHIDefragStatistics
    {
        RHISizeType moved_bytes_{ 0u };
        RHISizeType freed_bytes_{ 0u };
        uint32_t    freed_blocks_{ 0u };
        uint32_t    moved_allocations_{ 0u };
        auto& operator+=(const RHIDefragStatistics& other) {
            moved_bytes_ += other.moved_bytes_;
            freed_bytes_ += other.freed_bytes_;
            freed_blocks_ += other.freed_blocks_;
            moved_allocations_ += other.moved_allocations_;
            return *this;
        }
    };

    enum ERHIDefragFlagBits : uint32_t
    {
        eFast       = 0x1,
        eBalanced   = 0x2,
        eFull       = 0x4,
        eExtensive  = 0x8,
        eNeedState  = eBalanced|eExtensive,
        eMask       = eFast|eBalanced|eFull|eExtensive,
    };

    using RHIDefragFlags = uint32_t;

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
        RHIAllocation* src_{ nullptr };
        RHIAllocation* tmp_dst_{ nullptr };
    };

    struct DefragMoveBatchInfo
    {
        uint32_t count_{ 0u };
        DefragMove* moves_{ nullptr };
    };

    class MINIT_API RHIMemoryResidencyDefragExecutor final
    {
    public:
        RHIMemoryResidencyDefragExecutor(RHIMemoryResidencyManager* manager);
        ~RHIMemoryResidencyDefragExecutor();
        void BeginDefragPass(RHIMemoryResidencyManager& manager, DefragMoveBatchInfo& moves);
        void EndDefragPass(RHIMemoryResidencyManager& manager, DefragMoveBatchInfo& move_info);
        RHIDefragStatistics GetDefragPassStatistics()const { return global_stats_; }
        EMoveOp TestMoveOp(RHISizeType size) const;
        bool UpdatePassStatus(RHISizeType move_size);
        auto& GetMoves() { return moves_; }
        RHISizeType GetMovesCount() const { return moves_.size(); }
        auto* GetDefragState() { return (flags_ & ERHIDefragFlagBits::eNeedState) ? defrag_state_.get() : nullptr; }
    private:
        DISALLOW_COPY_AND_ASSIGN(RHIMemoryResidencyDefragExecutor);
        using DefragFunc = std::function<bool(RHIMemoryResidencyDefragExecutor&, RHIManagedMemoryBlockVector& vec, uint32_t index, bool update)>;
    private:
        RHIDefragFlags  flags_;
        RHIMemoryResidencyManager* const residency_manager_{ nullptr };
        RHISizeType pass_max_bytes_;
        RHISizeType pass_max_allocations_;
        RHIDefragStatistics global_stats_;
        RHIDefragStatistics pass_stats_;
        DefragFunc  defrag_func_;
        uint8_t max_ignore_allocs_count_;
        uint8_t ignore_allocs_count_{ 0u };
        uint32_t    immovavle_block_count_{ 0u };
        std::shared_ptr<void>   defrag_state_; 
        using DefragMoveAllocator = std::remove_pointer_t<decltype(g_rhi_allocator)>::rebind<DefragMove>::other;
        Vector<DefragMove, DefragMoveAllocator>  moves_{ *g_rhi_allocator };
    };
#endif

}
