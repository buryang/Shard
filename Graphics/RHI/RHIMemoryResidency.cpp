#include "RHIMemoryResidency.h"
#include <bit>

#ifdef DEBUG
#define MEMORY_DEBUG_MARGIN 16
#else
#define MEMORY_DEBUG_MARGIN 0
#endif

namespace Shard::RHI
{

    struct BlockRange
    {
        RHISizeType offset_;
        RHISizeType size_;
        BlockRange* physic_prev_{ nullptr };
        BlockRange* physic_next_{ nullptr };
        BlockRange* prev_free_{ nullptr };
        union
        {
            BlockRange* next_free_{ nullptr };
            void* user_data_;
        };
        FORCE_INLINE void MarkFree() { prev_free_ = nullptr; }
        FORCE_INLINE void MarkTaken() { prev_free_ = this; }
        FORCE_INLINE bool IsFree() const { return prev_free_ != this; }
        FORCE_INLINE BlockRange*& PrevFree() { return prev_free_; }
        FORCE_INLINE BlockRange*& NextFree() { return next_free_; }
        FORCE_INLINE bool IsMergeableWithPrev() const {
            return IsFree() && (physic_prev_ != nullptr && physic_prev_->IsFree());
        }
        FORCE_INLINE bool IsMergeableWithNext() const {
            return IsFree() && (physic_next_ != nullptr && physic_next_->IsFree());
        }
        OVERLOAD_OPERATOR_NEW(BlockRange);
    };

    namespace
    {
        struct AllocationRequest
        {
            RHIAllocHandle alloc_handle_{ nullptr };
            RHISizeType size_{ 0u };
            void* custom_data_;
            uint64_t alogrithm_data_;
        };

        //http://www.gii.upv.es/tlsf/files/ecrts04_tlsf.pdf
        class ALIGN_CACHELINE BlockMetaHeaderTLSF final
        {
            OVERLOAD_OPERATOR_NEW(BlockMetaHeaderTLSF);
        public:
            enum
            {
                SMALL_BUFFER_SIZE = 256,
                FIRST_LEVEL_SHITT = 7,
                INITIAL_BLOCK_ALLC_COUNT = 16,
                MAX_FIRST_LEVEL_INDEX = 65 - FIRST_LEVEL_SHITT,
                SECOND_LEVEL_INDEX = 5,
                MAX_FREE_LIST_NUM = MAX_FIRST_LEVEL_INDEX * SECOND_LEVEL_INDEX,
            };
            BlockMetaHeaderTLSF(RHISizeType size);
            ~BlockMetaHeaderTLSF();
            bool CreateAllocationRequest(RHISizeType alloc_size, RHISizeType alignment, bool up_address,
                                        uint32_t strategy, AllocationRequest* request);
            void AllocBlock(const AllocationRequest& request, RHISizeType alloc_size, void* private_data);
            void DeAllocBlock(RHIAllocHandle memory);
            void Reset();
            uint32_t GetAllocationCount()const;
            uint32_t GetFreeBlockRangeCount()const;
            RHISizeType Size()const { return size_; }
            RHISizeType GetSumFreeSize()const;
            uint32_t GetSumFreeRangeCount()const;
            bool IsEmpty()const;

            void AddStatistics(RHIMemroyStatistics& stats) const;
            void AddDetailedStatistics(RHIMemoryDetailStatistics& stats) const;

        private:
            DISALLOW_COPY_AND_ASSIGN(BlockMetaHeaderTLSF);
            uint32_t SizeToFirstLevel(RHISizeType sz) const;
            uint32_t SizeToSecondLevel(RHISizeType sz, uint32_t fl) const;
            uint32_t GetListIndex(uint32_t fl, uint32_t sl) const;
            uint32_t GetListIndex(RHISizeType sz) const;

            /*memory block ops*/
            void RemoveFreeBlock(BlockRange* block);
            void InsertFreeBlock(BlockRange* block);
            void MergeBlock(BlockRange* block, BlockRange* prev);
            BlockRange* FindFreeBlock(uint32_t& list_index, RHISizeType sz) const;
            bool CheckBlock(BlockRange& block, uint32_t list_index, RHISizeType alloc_size, RHISizeType alignment, AllocationRequest* request) const;
            bool CheckCorruption(const void* block_data) const;
        private:
            uint32_t    fl_{ 0u };
            uint32_t    sl_[MAX_FIRST_LEVEL_INDEX]{ 0u };

            //data storage
            BlockRange* null_block_{ nullptr };
            BlockRange* free_list_head_[MAX_FREE_LIST_NUM]{ nullptr };

            //
            uint32_t list_count_{ 0u };
            RHISizeType size_;
            RHISizeType alloc_count_;
            RHISizeType blocks_free_count_;
            RHISizeType blocks_free_size_;

        };


        /*memory statistic ops*/
        static inline void AddDetailedStatisticsUnusedRange(RHIMemoryDetailStatistics& stats, RHISizeType sz)
        {
            stats.unused_range_count_++;
            stats.unused_range_min_size_ = std::min(stats.unused_range_min_size_, sz);
            stats.unused_range_max_size_ = std::max(stats.allocation_max_size_, sz);
        }

        static void AddDetailedStatisticsAllocation(RHIMemoryDetailStatistics& stats, RHISizeType sz)
        {
            stats.stat_.allocation_count_++;
            stats.stat_.allocation_bytes_ += sz;
            stats.allocation_min_size_ = std::min(stats.allocation_min_size_, sz);
            stats.allocation_max_size_ = std::max(stats.allocation_max_size_, sz);
        }

        static constexpr uint32_t GetDebugMargin() { return MEMORY_DEBUG_MARGIN; }

        BlockMetaHeaderTLSF::BlockMetaHeaderTLSF(RHISizeType size)
        {
            null_block_ = new BlockRange;
            null_block_->size_ = size;
            
            auto fli = SizeToFirstLevel(size);
            auto sli = SizeToSecondLevel(size, fli);
            list_count_ = fli ? (fli - 1) * (1UL << SECOND_LEVEL_NUM) + sli + 1UL : 1UL;
            list_count_ += 4;

            //todo 

        }

        BlockMetaHeaderTLSF::~BlockMetaHeaderTLSF()
        {
            Reset();
            delete null_block_;
        }

        bool BlockMetaHeaderTLSF::CreateAllocationRequest(RHISizeType alloc_size, RHISizeType alignment, bool up_address, uint32_t strategy, AllocationRequest* request)
        {
            assert(alloc_size > 0 && "alloc size must not be empty");
            alloc_size += GetDebugMargin(); //todo debug margin
            if (alloc_size > GetSumFreeSize()) {
                return false;
            }

            auto size_for_next_list = alloc_size;
            auto small_size_step = SMALL_BUFFER_SIZE / 4;
            if (alloc_size > SMALL_BUFFER_SIZE) {
                size_for_next_list += (1ULL << (Utils::BitScanMSB(alloc_size) - SECOND_LEVEL_INDEX));
            }
            else if (alloc_size > SMALL_BUFFER_SIZE - small_size_step) {
                size_for_next_list = SMALL_BUFFER_SIZE + 1;
            }
            else {
                size_for_next_list += small_size_step;
            }

            uint32_t next_list_index{ 0u };
            uint32_t prev_list_index{ 0u };
            BlockRange* next_list_block{ nullptr };
            BlockRange* prev_list_block{ nullptr };

            const auto iterate_next_list = [&]() {
                while (next_list_block) {
                    if (CheckBlock(*next_list_block, next_list_index, alloc_size, alignment, request)) {
                        return true;
                    }
                    next_list_block = next_list_block->NextFree();
                }
                return false;
                };

            const auto iterate_prev_list = [&]() {
                while (prev_list_block) {
                    if (CheckBlock(*prev_list_block, prev_list_index, alloc_size, alignment, request)) {
                        return true;
                    }
                    prev_list_block = prev_list_block->PrevFree();
                }
                return false;
                };
            strategy &= Utils::EnumToInteger(ERHIMemoryFlagBits::eStrategyMask);
            strategy = strategy ? strategy : Utils::EnumToInteger(ERHIMemoryFlagBits::eStrategyMinTime); //set min time as default

            if (strategy & Utils::EnumToInteger(ERHIMemoryFlagBits::eStrategyMinTime)) {
                next_list_block = FindFreeBlock(next_list_index, size_for_next_list);
                if (next_list_block != nullptr && CheckBlock()) {
                    return true;
                }
                //check null block
                if (CheckBlock(*null_block_, list_count_, alloc_size, alignment, request))
                {
                    return true;
                }
                if (iterate_next_list()) {
                    return true;
                }
                prev_list_block = FindFreeBlock(prev_list_index, alloc_size);
                if (iterate_prev_list()) {
                    return true;
                }
            }
            else if (strategy & Utils::EnumToInteger(ERHIMemoryFlagBits::eStrategyMinOccupy)) {
                prev_list_block = FindFreeBlock(prev_list_index, alloc_size);
                if (iterate_prev_list()) {
                    return true;
                }

                //check null block
                next_list_block = FindFreeBlock(next_list_index, size_for_next_list);
                if (iterate_next_list()) {
                    return true;
                }
            }
            else if (strategy & Utils::EnumToInteger(ERHIMemoryFlagBits::eStrategyMinOffset)) {
                auto* block = null_block_->physic_prev_;
                while (block) {
                    block = block->physic_prev_;
                }
                for (auto n = 0; n < blocks_free_count_; ++n) {
                    if (block->IsFree() && block->size_ >= alloc_size && CheckBlock(*block, GetListIndex(block->size_), alloc_size, alignment, request)) {
                        return true;
                    }
                }
                return CheckBlock(*null_block_, list_count_, alloc_size, alignment, request);
            }

            //do full search
            while (++next_list_index < list_count_) {
                next_list_block = free_list_head_[next_list_index];
                if (iterate_next_list()) {
                    return true;
                }
            }
            
            //no suitable memory found
            return false;
        }

        void BlockMetaHeaderTLSF::AllocBlock(const AllocationRequest& request, RHISizeType alloc_size, void* private_data)
        {
            auto* curr_block = reinterpret_cast<BlockRange*>(request.alloc_handle_);
            assert(curr_block != nullptr);
            if (curr_block != null_block_) {
                RemoveFreeBlock(curr_block);
            }
            //deal with missalignment
            auto missing_alignment = offset - curr_block->offset_;
            if (missing_alignment) {

            }

            auto size = request.size_ + GetDebugMargin();
            if (size == curr_block->size_ && curr_block == null_block_) {
                null_block_ = new BlockRange;
                null_block_->offset_ = curr_block->offset_ + size;
                null_block_->physic_prev_ = curr_block;
                null_block_->MarkFree();
                curr_block->physic_next_ = null_block_;
                curr_block->MarkTaken();
            }
            else
            {
                assert(curr_block->size_ > size);
                auto* new_block = new BlockRange;
                new_block->size_ = curr_block->size_ - size;
                new_block->offset_ = curr_block->offset_ + size;
                new_block->physic_prev_ = curr_block;
                new_block->physic_next_ = std::exchange(curr_block->physic_next_, new_block);
                curr_block->size_ = size;
                if (curr_block == null_block_) {
                    null_block_ = new_block;
                    null_block_->MarkFree();
                    curr_block->MarkTaken();
                }
                else
                {
                    new_block->physic_next_->physic_prev_ = new_block;
                    new_block->MarkFree();
                    InsertFreeBlock(new_block);
                }
            }

            if (GetDebugMargin() > 0) {

            }
            ++alloc_count_;
        }

        void BlockMetaHeaderTLSF::DeAllocBlock(RHIAllocHandle memory)
        {
            auto* block = (BlockRange*)memory;
            auto* next = block->physic_next_;
            --alloc_count_;

            //try to merge
            if (block->IsMergeableWithPrev()) {
                auto* prev = block->physic_prev_;
                RemoveFreeBlock(prev);
                MergeBlock(block, prev);
            }

            if (block->IsMergeableWithNext()) {
                if (next == null_block_) {
                    MergeBlock(null_block_, block);
                }
                else
                {
                    RemoveFreeBlock(next);
                    MergeBlock(next, block);
                    InsertFreeBlock(next);
                }
            }
            else
            {
                InsertFreeBlock(block);
            }
        }

        void BlockMetaHeaderTLSF::Reset()
        {
            while (auto* block = null_block_->physic_prev_)
            {
                delete std::exchange(block, block->physic_prev_);
            }
            *null_block_ = {};
            
        }

        uint32_t BlockMetaHeaderTLSF::GetAllocationCount() const
        {
            return alloc_count_;
        }

        RHISizeType BlockMetaHeaderTLSF::GetSumFreeSize() const
        {
            return blocks_free_size_ + null_block_->size_;
        }

        uint32_t BlockMetaHeaderTLSF::GetSumFreeRangeCount() const
        {
            return blocks_free_count_ + 1u; //1 for null block
        }

        bool BlockMetaHeaderTLSF::IsEmpty() const
        {
            return false;
        }

        void BlockMetaHeaderTLSF::AddStatistics(RHIMemroyStatistics& stats) const
        {
            stats.block_count_++;
            stats.block_bytes_ += size_;
            stats.allocation_count_ += alloc_count_;
            stats.allocation_bytes_ += 0u;
        }

        void BlockMetaHeaderTLSF::AddDetailedStatistics(RHIMemoryDetailStatistics& stats) const
        {
            AddStatistics(stats.stat_);
            for (auto* block = null_block_->physic_prev_; block != nullptr; block = block->physic_prev_)
            {
                if (block->IsFree())
                {
                    AddDetailedStatisticsUnusedRange(stats, block->size_);
                }
                else
                {
                    AddDetailedStatisticsAllocation(stats, block->size_);
                }
            }

            if (null_block_->size_ > 0u) {
                AddDetailedStatisticsUnusedRange(stats, null_block_->size_);
            }
        }

        uint32_t BlockMetaHeaderTLSF::SizeToFirstLevel(RHISizeType sz) const
        {
            if (sz > SMALL_BUFFER_SIZE) {
                return Utils::BitScanMSB(sz) - FIRST_LEVEL_SHITT;
            }
            return 0u;
        }

        uint32_t BlockMetaHeaderTLSF::SizeToSecondLevel(RHISizeType sz, uint32_t fl) const
        {
            if (!fl) {
                return static_cast<uint32_t>(sz >> (fl + FIRST_LEVEL_SHITT)); //todo
            }
        }

        uint32_t BlockMetaHeaderTLSF::GetListIndex(uint32_t fl, uint32_t sl) const
        {
            return fl * SECOND_LEVEL_INDEX + sl;
        }

        uint32_t BlockMetaHeaderTLSF::GetListIndex(RHISizeType sz) const
        {
            const auto fl = SizeToFirstLevel(sz);
            const auto sl = SizeToSecondLevel(sz, fl);
            return GetListIndex(fl, sl);
        }

        void BlockMetaHeaderTLSF::RemoveFreeBlock(BlockRange* block)
        {
            if (block->NextFree() != nullptr)
            {
                block->NextFree()->PrevFree() = block->PrevFree();
            }
            if (block->PrevFree() != nullptr)
            {
                block->PrevFree()->NextFree() = block->NextFree();
            }
            else
            {
                const auto list_index = GetListIndex(block->size_);
                free_list_head_[list_index] = block->NextFree();
            }
            --blocks_free_count_;
            blocks_free_size_ -= block->size_;
        }

        void BlockMetaHeaderTLSF::InsertFreeBlock(BlockRange* block)
        {
            const auto list_index = GetListIndex(block->size_);
            block->MarkFree();
            block->NextFree() = std::exchange(free_list_head_[list_index], block);

            if (block->NextFree() != nullptr) {
                block->NextFree()->PrevFree() = block;
            }
            else
            {

            }
            ++blocks_free_count_;
            blocks_free_size_ += block->size_;
        }

        void BlockMetaHeaderTLSF::MergeBlock(BlockRange* block, BlockRange* prev)
        {
            if (block->physic_prev_ == prev && block->IsMergeableWithPrev())
            {
                block->offset_ = prev->offset_;
                block->size_ += prev->size_;
                block->physic_prev_ = prev->physic_prev_;
                if (block->physic_prev_) {
                    block->physic_prev_->physic_next_ = block;
                }

                delete prev;
            }
        }

        BlockRange* BlockMetaHeaderTLSF::FindFreeBlock(uint32_t& list_index, RHISizeType sz) const
        {
            auto fli = SizeToFirstLevel(sz);
            auto inner_free_map = 0u;
            if (!inner_free_map) {
                //to try all higher levels
                auto free_map = fl_ & (~0UL << (fli+1));
                if (!free_map) {
                    return nullptr;
                }
                fli = Utils::BitScanLSB(free_map);
                inner_free_map = sl_[fli];
                assert(inner_free_map != 0u);
            }
            list_index = GetListIndex(fli, Utils::BitScanLSB(inner_free_map));
            return free_list_head_[list_index];
        }

        bool BlockMetaHeaderTLSF::CheckBlock(BlockRange& block, uint32_t list_index, RHISizeType alloc_size, RHISizeType alignment, AllocationRequest* request) const
        {
            if (!block.IsFree()) {
                return false;
            }

            const auto aligned_offset = Utils::AlignUp(block.offset_, alignment);
            if (block.size_ < alloc_size + aligned_offset - block.offset_) {
                return false;
            }

            //move block to head, to avoid second time check
            if (list_index != list_count_ && block.PrevFree()) {
                block.PrevFree()->NextFree() = block.NextFree();
                if (block.NextFree()) {
                    block.NextFree()->PrevFree() = block.PrevFree();
                }
                block.PrevFree() = nullptr;
                block.NextFree() = std::exchange(free_list_head_[list_index], &block);
                if (block.NextFree()) {
                    block.NextFree()->PrevFree() = &block;
                }
                
            }
            return true;
        }

        bool BlockMetaHeaderTLSF::CheckCorruption(const void* block_data) const
        {
            const auto validate_magic_value = [](const void* block_data, RHISizeType pffset) {

                return true;
            };
            while (auto* block = null_block_->physic_prev_)
            {
                if (!block->IsFree()) {
                    if (validate_magic_value(block_data, block->size_ + block->offset_)) {
                        return false;
                    }
                }
            }
            return true;
        }
    }

    RHIManagedMeomoryBlockVector::RHIManagedMeomoryBlockVector(RHIMemoryResidencyManager* const manager, const uint32_t min_block_count, const uint32_t max_block_count, 
        const RHISizeType preferred_block_size, const RHISizeType expected_block_size, const float priority, EAllocateStrategy algoritm) :manager_(manager), min_block_count_(min_block_count), max_block_count_(max_block_count), preferred_block_size_(preferred_block_size), priority_(std::clamp(priority, 0.f, 1.f))
    {
        assert(manager != nullptr && "vector must has one valid parent");
        assert(min_block_count < max_block_count && min_block_count >= 0);
        for (auto n = 0; n < min_block_count_; ++n) {
            RHIManangedMemoryBlock* ptr{ nullptr };
            if (CreateBlock(preferred_block_size_, ptr)) {
                blocks_.emplace_back(ptr);
            }
        }
    }

    void RHIManagedMeomoryBlockVector::Alloc(RHISizeType size, RHIManagedMemory*& memory)
    {
        memory = nullptr;
        const auto alloc_size = size + GetDebugMargin();
        if ( alloc_size > preferred_block_size_) {
            return;
        }

        RHIManangedMemoryBlock* parent_block{ nullptr };
        for (auto* block : blocks_) {
            const auto* block_head = reinterpret_cast<BlockMetaHeaderTLSF*>(block->header_);
            if (block_head->GetSumFreeRangeCount() > alloc_size && ) {
                parent_block = block;
            }
        }
        
        const auto test_new_block = GetTotalBlockCount() < max_block_count_;
        if (parent_block == nullptr&&test_new_block) {
            //alloc a new block

            CreateBlock();
        }
        
        if(parent_block != nullptr){
            memory = new RHIManagedMemory;
            AllocFromBlock(parent_block, alloc_size, 0u, &memory);
        }
        
    }

    void RHIManagedMeomoryBlockVector::DeAlloc(RHIManagedMemory* memory)
    {
    }

    bool RHIManagedMeomoryBlockVector::CreateBlock(RHISizeType block_size, RHIManangedMemoryBlock*& block)
    {
        RHIManagedMemoryCreateInfo block_info;
        const auto heap_index = manager_->FindMemoryTypeIndex(block_info);

        //todo
        block = new RHIManangedMemoryBlock;
        block->header_ = new BlockMetaHeaderTLSF(block_size);
        block->rhi_mem_ = manager_->MallocRawMemory(, flags, sz);
        return block != nullptr;
    }

    void RHIManagedMeomoryBlockVector::RemoveBlock(RHIManangedMemoryBlock* block)
    {
        std::remove_if(blocks_.begin(), blocks_.end(), [block](const auto* curr_block)->bool {return curr_block == block; });
    }

    RHISizeType RHIManagedMeomoryBlockVector::CalcTotalBlockSize() const
    {
        RHISizeType total_size{ 0u };
        std::for_each(blocks_.cbegin(), blocks_.cend(), [&total_size](auto block) { total_size += reinterpret_cast<BlockMetaHeaderTLSF*>(block->header_)->Size(); });
        return total_size;
    }

    RHISizeType RHIManagedMeomoryBlockVector::CalcMaxBlockSize() const
    {
        RHISizeType max_size{ 0u };
        for (const auto* block : blocks_) {
            max_size = std::max(max_size, reinterpret_cast<BlockMetaHeaderTLSF*>(block->header_)->Size());
            if (max_size >= preferred_block_size_) {
                break;
            }
        }
        return max_size;
    }

    RHIManagedMeomoryBlockVector::~RHIManagedMeomoryBlockVector()
    {
        for (auto* block : blocks_) {
            assert(block != nullptr);
            //todo destroy block
        }
        blocks_.clear();
    }

    bool RHIManagedMeomoryBlockVector::AllocFromBlock(RHIManangedMemoryBlock* block, RHISizeType size, RHISizeType alignment, RHIManagedMemory* memory)
    {
        return false;
    }

    bool RHIManagedMeomoryBlockVector::CommitAllocationRequest(RHIManangedMemoryBlock* block, void* user_data)
    {
        return false;
    }

    RHIDedicatedMemoryBlock* RHIMemoryResidencyManager::AllocDedicatedBlock(const RHIManagedMemoryCreateInfo& mem_info)
    {
        return nullptr;
    }

    RHIManagedMemory* RHIMemoryResidencyManager::AllocManagedMemory(const RHIManagedMemoryCreateInfo& mem_info)
    {
        //block_meta = reinterpret_cast<BlockMetaHeaderTLSF*>(parent->header_);
        AllocationRequest curr_request{};
        if (block_meta->CreateAllocationRequest()) {
            //todo
        }
        return nullptr;
    }

    void RHIMemoryResidencyManager::Tick(float time)
    {
        curr_time_stamp_ = time;
        curr_frame_index_++; //todo 
    }

    void RHIMemoryResidencyManager::MakeResident(RHI::RHIResource::Ptr res_ptr)
    {
        const auto mem_info = GetResourceResidencyInfo(res_ptr);
        if (mem_info.IsDedicated()) {
            auto* dedicate_mem = AllocDedicatedBlock(mem_info);
            MakeResident(res_ptr, dedicate_mem);
        }
        else
        {
            auto* managed_mem = AllocManagedMemory(mem_info);
            MakeResident(res_ptr, managed_mem, 0u, mem_info.Size());
        }
    }

    const RHIMemBudget& RHIMemoryResidencyManager::GetMemoryBudget(uint8_t heap_index) const
    {
        return mem_budget_.heap_budget_[heap_index];
    }

    const RHIMemoryDetailStatistics& RHIMemoryResidencyManager::GetMemoryStatistics(uint8_t heap_index) const
    {
        return heap_index == (uint8_t)-1 ? mem_statistics_.total_ : mem_statistics_.heap_details_[heap_index];
    }
  
#if 1 //def USE_MEMORY_DEFRAG
    /*defag need inner struct*/
    namespace
    {
        struct StateBlanced
        {
            RHISizeType avg_free_size_{ 0u };
            RHISizeType avg_alloc_size_{ std::numeric_limits<RHISizeType>::max() };
        };
        
        struct MoveRangeData
        {
            RHISizeType size_{ 0u };
            RHISizeType alignment_{ 0u };
            DefragMove move_;
        };

        static bool ReAllocWithInBlock(,RHIManangedMemoryBlock* block) {
            auto* block_meta = block->header_;
        }
        static bool AllocInOtherBlock(RHISizeType start, RHISizeType end, MoveRangeData& data, ) {

        }
        static bool DoDefragFast(RHIManagedMeomoryBlockVector& vec) {

        }
        static bool DoDefragBalanced(RHIManagedMeomoryBlockVector& vec) {

        }
        static bool DoDefragExtensive(RHIManagedMeomoryBlockVector& vec) {

        }
    }

    RHIMemoryResidencyDefragExecutor::RHIMemoryResidencyDefragExecutor(RHIMemoryResidencyManager* manager):residency_manager_(manager)
    {
        assert(manager != nullptr);
        pass_max_bytes_ = GET_PARAM_TYPE_VAL(UINT, RESIDENCY_DEFRAG_MAX_BYTES);
        if (!pass_max_bytes_) {
            pass_max_bytes_ = std::numeric_limits<RHISizeType>::max();
        }
        pass_max_allocations_ = GET_PARAM_TYPE_VAL(UINT, RESIDENCY_DEFRAG_MAX_ALLOCATIONS);
        if (!pass_max_allocations_) {
            pass_max_allocations_ = std::numeric_limits<uint32_t>::max();
        }
        const auto strategy = GET_PARAM_TYPE_VAL(UINT, RESIDENCY_DEFRAG_STRATEGY);
        if (strategy == EnumToInteger(EFlags::eBlance)) {
            defrag_state_ = std::make_shared<StateBlanced>();
        }
    }

    void RHIMemoryResidencyDefragExecutor::BeginDefragPass(RHIMemoryResidencyManager& manager, DefragMoveBatchInfo& moves)
    {
    }

    void RHIMemoryResidencyDefragExecutor::EndDefragPass(RHIMemoryResidencyManager& manager, const DefragMoveBatchInfo& moves)
    {
    }

    RHIDefragStatistics RHIMemoryResidencyDefragExecutor::GetDefragPassStatistics() const
    {
        return RHIDefragStatistics();
    }
#endif
}
