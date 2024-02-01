#pragma once
#include "Utils/CommonUtils.h"
#include "RHIResources.h"

namespace Shard::RHI
{
    enum ERHIMemoryTypeFlagBits : uint16_t
    {

    };
    using RHIMemoryTypeFlags = uint16_t;

    enum ERHIMemoryHeapFlagBits : uint16_t
    {
        eDeviceLocal = 0x1,
        eHostVisible = 0x2, //system memory, uncached, GPU can access it
        eHostCached  = 0x4, //system memory, cached, GPU reads snoop CPU cache
        eHostCoherent = 0x8,
    };
    using RHIMemoryHeapFlags = uint16_t;

    enum
    {
        MAX_HEAP_COUNT = 16,
    };

    /** small limit on maximum number of allocations */
    static constexpr uint32_t MEMORY_MAX_ALLOCATIONS = 4096u; 
    extern   std::atomic_uint32_t g_allocation_counter;

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
        enum
        {
            DEDICATE_FLAGS_BIT = 0x1,
        };
        union
        {
            struct
            {
                RHIMemoryTypeFlags  type_;
                RHIMemoryHeapFlags  heap_;
            } mem_flags_;
            uint32_t    packed_flags_{ 0u };
        };
        uint64_t    alignment_{ 0u };
        uint64_t    size_{ 0u }; //assume size be 2^x
        //rhi spec user data
        void* user_data_{ nullptr };

        FORCE_INLINE bool IsDedicated()const { return size_ & DEDICATE_FLAGS_BIT; }
        FORCE_INLINE uint64_t Size()const { return size_ & ~DEDICATE_FLAGS_BIT; }
    };

    struct RHIManangedMemoryBlock;
    struct RHIManagedMemory
    {
        enum class EState
        {
            eResident,
            eEvicted,
        };
        RHIManangedMemoryBlock* parent_{ nullptr };
        EState  state_{ EState::eEvicted };
        uint64_t    offset_{ 0u };
        uint64_t    last_time_stamp_{ 0u };
        RHIManagedMemoryCreateInfo  info_:
    };

    struct RHIManangedMemoryBlock
    {
        std::atomic_uint32_t    ref_count_{ 0u };
        uint64_t                size_{ 0u };
        List<RHIManagedMemory*> consumers_;
        void*                   rhi_mem_;
    };

    struct RHIDedicatedMemoryBlock
    {
        void* rhi_mem_;
        uint64_t    last_time_stamp_{ 0u };
    };

    //todo
    struct RHIVirtualMemoryBlock
    {

    };

    struct RHIMemoryDetailStatistics
    {
        uint64_t    usage_;
        uint64_t    peak_;
        uint64_t    budget_;
    };

    struct RHIMemoryTotalStatistics
    {
        RHIMemoryDetailStatistics   heap_details_[MAX_HEAP_COUNT];
        uint32_t    total_allocation_count_{ 0u }; //vulkan max 4096
    };

    struct RHIMemBudget
    {
        uint64_t    usage_;
        uint64_t    budget_;
    };

    //http://www.gii.upv.es/tlsf/files/ecrts04_tlsf.pdf
    class RHIMemoryResidencyManager
    {
    public:
        using Ptr = RHIMemoryResidencyManager*;
        /**
         * \brief get resource memory information from rhi
         * \param res_ptr
         * \return 
         */
        virtual RHIManagedMemoryCreateInfo GetResourceResidencyInfo(RHI::RHIResource::Ptr res_ptr) const = 0;
        /*for create_info has platform special user_data, so must implement this function each platform respectively*/
        virtual bool IsManagedMemoryCreateInfoCompatible(const RHIManagedMemoryCreateInfo& lhs, const RHIManagedMemoryCreateInfo& rhs) const { return true; }
        virtual RHIManangedMemoryBlock* AllocManagedBlock(RHIMemoryTypeFlags type, RHIMemoryHeapFlags heap, uint64_t sz) = 0;
        virtual RHIDedicatedMemoryBlock* AllocDedicatedBlock(const RHIManagedMemoryCreateInfo& mem_info) = 0;
        /**
         * \brief allocate one memory from prev alloc block,if block is nullptr, alloc one
         * \param block memory alias block
         * \return memory
         */
        virtual RHIManagedMemory* AllocManagedMemory(const RHIManagedMemoryCreateInfo& mem_info, RHIManangedMemoryBlock* parent=nullptr) = 0;
        virtual void CalcMemoryStatistics(RHIMemoryTotalStatistics& stat) const = 0;
        /**
         * \brief make a resource resident on managed memory
         * \param res_ptr
         * \param managed_mem
         */
        virtual void MakeResident(RHI::RHIResource::Ptr res_ptr, RHIManagedMemory* managed_mem) = 0;
        virtual void MakeResident(RHI::RHIResource::Ptr res_ptr, RHIDedicatedMemoryBlock* dedicated_mem) = 0;
        virtual void Evict(RHI::RHIResource::Ptr res_ptr) = 0;
        virtual ~RHIMemoryResidencyManager() = default;
    protected:
        virtual void TrimEvictMemory();
        virtual void GetMemoryBudget(RHIMemBudget& budget, RHIMemoryTypeFlags type, RHIMemoryHeapFlags heap) const;
        virtual void* MallocRawMemory(, , uint64_t size) = 0;
    protected:
        void BeginMemoryBlockTracking();
        void EndMemoruBlockTracking();
    protected:
        static std::atomic_bool  mem_lock_{ false };
        static SmallVector<List<RHIDedicatedMemoryBlock*> >   dedicated_list_; //todo
        static SmallVector<List<RHIManangedMemoryBlock*> >    managed_list_; //todo
    };

}
