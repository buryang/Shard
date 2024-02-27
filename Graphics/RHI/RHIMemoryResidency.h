#pragma once
#include "Utils/CommonUtils.h"
#include "Core/EngineGlobalParams.h"
#include "RHIResources.h"

namespace Shard::RHI
{
    /** whether prefer to alloca dedicated memory block */
    REGIST_PARAM_TYPE(BOOL, RESIDENCY_PREFER_DEDICATED, false);
    REGIST_PARAM_TYPE(UINT, RESIDENCY_PREFER_BLOCK_SIZE, 64 * 1024 * 1024);
    REGIST_PARAM_TYPE(UINT, RESIDENCY_ALLOCATE_STRATEGY, EAllocateStrategy::eTLSF);
    REGIST_PARAM_TYPE(UINT, RESIDENCY_DEFRAG_STRATEGY, EDefrag)
    REGIST_PARAM_TYPE(UINT, RESIDENCY_DEFRAG_MAX_BYTES, 1);
    REGIST_PARAM_TYPE(UINT, RESIDENCY_DEFRAG_MAX_ALLOCATIONS, 12);

    using RHIAllocHandle = uintptr_t;
    using RHISizeType = uint64_t;

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
        eStrategyMask       = eStrategyMinOffet | eStrategyMinTime | eStrategyMinOccupy,

        //dedicated flag bits
        eForceDedicated     = 0x100,
        //mapped when allocated
        eAllocMapped        = 0x200,
        ePersistentMap      = 0x400,
        eAllowMap           = 0x800,
        eMapMak             = eAllocMapped | ePersistentMap | eAllowMap,

        //check whether allocation in budget
        eWithInBudget       = 0x1000,
    };

    using RHIMemoryFlags = uint32_t;

    enum ERHIDefragFlagBits : uint8_t
    {
        eNone           = 0x0,
        eFull           = 0x1,
        eFast           = 0x2,
        eExtensive      = 0x4,
    };
    using RHIDefragFlags = uint8_t;

    enum
    {
        MAX_HEAP_COUNT = 32u,
        /*small limit on maximum number of allocations */
        MEMORY_MAX_ALLOCATIONS = 4096u,
    };

    extern std::atomic_uint32_t g_allocation_counter;

    FORCE_INLINE uint32_t GetMemAllocationCount() {
        return g_allocation_counter.load();
    }
    FORCE_INLINE uint32_t IncrAllocationCount() {
        return g_allocation_counter.fetch_add(1u);
    }
    FORCE_INLINE uint32_t DiscrAllocationCount() {
        return g_allocation_counter.fetch_sub(1u);
    }
    FORCE_INLINE bool IsAllocationExceedLimit() {
        return GetMemAllocationCount() > MEMORY_MAX_ALLOCATIONS;
    }

    struct RHIManagedMemoryCreateInfo
    {
        OVERLOAD_OPERATOR_NEW(RHIManagedMemoryCreateInfo);
        enum
        {
            DEDICATE_FLAGS_BIT = 0x1,
        };
        union
        {
            struct
            {
                RHIMemoryUsage  type_;
                RHIMemoryFlags  flags_;
            } mem_flags_;
            uint64_t    packed_flags_{ 0u };
        };
        RHISizeType    alignment_{ 0u };
        RHISizeType    size_{ 0u }; //assume size be 2^x
        //rhi spec user data
        void*   user_data_{ nullptr };

        FORCE_INLINE bool IsDedicated()const { return size_ & DEDICATE_FLAGS_BIT; }
        FORCE_INLINE RHISizeType Size()const { return size_ & ~DEDICATE_FLAGS_BIT; }
    };

    struct RHIManagedMemoryBlockCreateInfo
    {
    };

    struct RHIManangedMemoryBlock;
    struct RHIManagedMemory
    {
        OVERLOAD_OPERATOR_NEW(RHIManagedMemory);
        RHIManangedMemoryBlock* parent_{ nullptr };
        RHISizeType    offset_{ 0u };
        RHISizeType    alignment_{ 0u };
        RHISizeType    size_{ 0u }; //assume size be 2^x
       
        FORCE_INLINE bool IsMapped() const { return mapped_ != nullptr; }
    };

    struct RHIManangedMemoryBlock
    {
        OVERLOAD_OPERATOR_NEW(RHIManangedMemoryBlock);
        void*   header_{ nullptr };
        RHISizeType size_{ 0u };
        void*   rhi_mem_{ nullptr };
    };

    struct RHIDedicatedMemoryBlock
    {
        OVERLOAD_OPERATOR_NEW(RHIDedicatedMemoryBlock);
        void* rhi_mem_;
    };

    struct RHIAllocationMemory
    {
        union {
            struct
            {
                RHIManagedMemory    mem_;
                RHISizeType offset_;
            }managed_mem_;
            RHIDedicatedMemoryBlock dedicated_mem_;
        };
        uint64_t    last_time_stamp_{ 0u };
        void* mapped_{ nullptr };
        FORCE_INLINE bool IsMapped() const { return mapped_ != nullptr; }
    };

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
        RHIMemoryDetailStatistics   heap_details_[MAX_HEAP_COUNT];
        RHIMemoryDetailStatistics   total_;
        void* platform_spec_{ nullptr };
    };

    struct RHIMemBudget
    {
        RHIMemroyStatistics stat_;

        /** region get from backend*/
        RHISizeType    usage_; //used size in bytes
        RHISizeType    budget_; //available size in bytes
    };

    struct RHIMemoryTotalBudget
    {
        RHIMemBudget    heap_budget_[MAX_HEAP_COUNT];
    };

    enum class EAllocateStrategy
    {
        eTLSF,
        eLinear, //to implement
    };

    class RHIMemoryResidencyManager;
    class RHIManagedMeomoryBlockVector 
    {
    public:
        friend class RHIMemoryResidencyDefragExecutor;
        RHIManagedMeomoryBlockVector(RHIMemoryResidencyManager* const manager, const uint32_t min_block_count, const uint32_t max_block_count,
                                     const RHISizeType preferred_block_size, const RHISizeType expected_block_size, const float priority, EAllocateStrategy algoritm);
        void Alloc(RHISizeType size, RHIManagedMemory*& memory);
        void DeAlloc(RHIManagedMemory* memory);
        bool CreateBlock(RHISizeType block_size, RHIManangedMemoryBlock*& block);
        void RemoveBlock(RHIManangedMemoryBlock* block);
        uint32_t GetTotalBlockCount() const { return blocks_.size(); }
        RHISizeType CalcTotalBlockSize() const;
        RHISizeType CalcMaxBlockSize()const;
        void SortBlocksByFreeSize();
        bool IsEmpty()const { return blocks_.empty(); }
        ~RHIManagedMeomoryBlockVector();
    private:
        bool AllocFromBlock(RHIManangedMemoryBlock* block, RHISizeType size, RHISizeType alignment, RHIManagedMemory* memory);
        bool CommitAllocationRequest(RHIManangedMemoryBlock* block, void* user_data);
    private:
        const float priority_{ 0.f };
        const uint32_t    min_block_count_{ 0u };
        const uint32_t    max_block_count_{ std::numeric_limits<uint32_t>::max() };
        const RHISizeType preferred_block_size_{ 0u };
        const RHISizeType expected_block_size_{ 0u };
        const EAllocateStrategy strategy_{ EAllocateStrategy::eTLSF };
        SmallVector<RHIManangedMemoryBlock*> blocks_;
        RHIMemoryResidencyManager* const manager_{ nullptr };
        mutable std::atomic_bool    lock_{ false };
    };

    class RHIDedicatedMemoryBlockList
    {
    public:
        friend class RHIMemoryResidencyDefragExecutor;
        RHIDedicatedMemoryBlockList(RHIDedicatedMemoryBlockList* const manager);
        void Alloc(RHISizeType size, RHIDedicatedMemoryBlock*& memory);
        void DeAlloc(RHIDedicatedMemoryBlock* memory);
        ~RHIDedicatedMemoryBlockList();
    private:
        struct DedicateNode{
            RHIDedicatedMemoryBlock* ptr_{ nullptr };
            DedicateNode* prev_{ nullptr };
            DedicateNode* nexet_{ nullptr };
        };
        DedicateNode* blocks_;
        RHIDedicatedMemoryBlockList* const manager_{ nullptr };
        mutable std::atomic_bool lock_{ false };
    };

    class MINIT_API RHIMemoryResidencyManager
    {
    public:
        friend class RHIMemoryResidencyDefragExecutor;
        friend class RHIManagedMeomoryBlockVector;
        friend class RHIDedicatedMemoryBlockList;

        using Ptr = RHIMemoryResidencyManager*;
        
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
        virtual RHIManagedMemoryCreateInfo GetResourceResidencyInfo(RHIResource::Ptr res_ptr) const = 0;
        virtual bool IsUMA() const { return false;  }
        /*for create_info has platform special user_data, so must implement this function each platform respectively*/
        bool IsManagedMemoryCreateInfoSupported(const RHIManagedMemoryCreateInfo& create_info) const { return FindMemoryTypeIndex(create_info.mem_flags_.type_, create_info.mem_flags_.flags_) == -1; }
        RHIDedicatedMemoryBlock* AllocDedicatedBlock(const RHIManagedMemoryCreateInfo& mem_info);
        /**
         * \brief allocate one memory from prev alloc block,if block is nullptr, alloc one
         * \param mem_info
         * \return memory
         */
        RHIManagedMemory* AllocManagedMemory(const RHIManagedMemoryCreateInfo& mem_info);
        uint32_t GetHeapCount()const { return managed_vec_.size(); }
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
        virtual void Evict(RHI::RHIResource::Ptr res_ptr) = 0;
        virtual ~RHIMemoryResidencyManager() = default;

        void Tick(float time);
    private:
        DISALLOW_COPY_AND_ASSIGN(RHIMemoryResidencyManager);
    protected:
        //void CommitAllocationRequst(void* private_data, RHIManagedMemory** managed_memory);
        const RHIMemBudget& GetMemoryBudget(uint8_t heap_index) const;
        const RHIMemoryDetailStatistics& GetMemoryStatistics(uint8_t heap_index) const;
        virtual void CalcMemoryStatistics(RHIMemoryDetailStatistics& statistic, uint32_t heap_index) const = 0;
        virtual uint8_t FindMemoryTypeIndex(RHIMemoryUsage usage, RHIMemoryFlags) const = 0;
        virtual RHISizeType GetMemoryHeapPreferredSize(uint8_t heap_index) = 0;
        //virtual void CalcDeviceBudget(RHIMemoryTotalBudget& budget) = 0;
        virtual void* MallocRawMemory(uint32_t flags, uint64_t size, void* plat_data = nullptr, void* user_data = nullptr) = 0;
        /**/
        virtual void FreeRawMemory(void* memory) = 0;
    protected:
        static std::atomic_bool global_lock_;
        static Array<RHIManagedMeomoryBlockVector*, MAX_HEAP_COUNT>   dedicated_vec_; //todo d3d12 committed 
        static Array<RHIDedicatedMemoryBlockList*, MAX_HEAP_COUNT>    managed_vec_; //todo d3d12 placed
        static uint64_t curr_time_stamp_;
        static uint64_t curr_frame_index_;
        static RHIMemoryTotalBudget mem_budget_;
        static RHIMemoryTotalStatistics mem_statistics_;
    };

    
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

#if 1//def USE_MEMORY_DEFRAG

    enum class EMoveOp
    {
        eCopy,
        eIgnore,
        eDestroy,
    };

    struct DefragMove
    {
        EMoveOp op_{ EMoveOp::eCopy };
        RHIManagedMemory* src_{ nullptr };
        RHIManagedMemory* dst_{ nullptr };
    };

    struct DefragMoveBatchInfo
    {
        uint32_t count_{ 0u };
        DefragMove* moves_{ nullptr };
    };

    class MINIT_API RHIMemoryResidencyDefragExecutor final
    {
    public:
        enum class EFlags
        {
            eFast,
            eBlance,
            eExtensive,
        };
        RHIMemoryResidencyDefragExecutor(RHIMemoryResidencyManager* manager);
        void BeginDefragPass(RHIMemoryResidencyManager& manager, DefragMoveBatchInfo& moves);
        void EndDefragPass(RHIMemoryResidencyManager& manager, const DefragMoveBatchInfo& moves);
        RHIDefragStatistics GetDefragPassStatistics()const { return global_stats_; }
    private:
        RHIMemoryResidencyManager* residency_manager_{ nullptr };
        RHISizeType pass_max_bytes_;
        RHISizeType pass_max_allocations_;
        RHIDefragStatistics global_stats_;
        RHIDefragStatistics pass_stats_;
        std::shared_ptr<void> defrag_state_;
    };
#endif

}
