#include "RHIMemoryResidency.h"
#include <bit>

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


        class MappingHysteresis
        {
        public:
            enum {
                COUNTER_MIN_EXTRA_MAPPING = 7,
            };
            uint32_t GetExtraMapping() const { return extra_mapping_; }
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

        private:
             
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
            RHISizeType Size()const { return size_; }
            RHISizeType GetSumFreeSize()const;
            uint32_t GetSumFreeRangeCount()const;
            RHIAllocHandle GetFirstAlloction() const;
            RHIAllocHandle GetNextAllocation(RHIAllocHandle curr) const;
            bool IsEmpty()const;
            RHISizeType GetAllocationOffset(RHIAllocHandle handle) const { return ((BlockRange*)handle)->offset_; }
            void*& GetAllocationPrivateData(RHIAllocHandle handle) { auto* block = ((BlockRange*)handle); assert(!block->IsFree()); return block->user_data_; }
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
            bool CheckCorruption() const;
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

        static FORCE_INLINE BlockMetaHeaderTLSF* GetBlockMeta(RHIManagedMemoryBlock& block) {
            return reinterpret_cast<BlockMetaHeaderTLSF*>(block.header_);
        }
        /*memory statistic ops*/
        static inline void AddDetailedStatisticsUnusedRange(RHIMemoryDetailStatistics& stats, RHISizeType sz)
        {
            stats.unused_range_count_++;
            stats.unused_range_min_size_ = std::min(stats.unused_range_min_size_, sz);
            stats.unused_range_max_size_ = std::max(stats.allocation_max_size_, sz);
        }

        static inline void AddDetailedStatisticsAllocation(RHIMemoryDetailStatistics& stats, RHISizeType sz)
        {
            stats.stat_.allocation_count_++;
            stats.stat_.allocation_bytes_ += sz;
            stats.allocation_min_size_ = std::min(stats.allocation_min_size_, sz);
            stats.allocation_max_size_ = std::max(stats.allocation_max_size_, sz);
        }
        static inline void AddBudgetAllocation(RHIMemoryBudget& budget, RHISizeType sz)
        {
            budget.stat_.allocation_bytes_ += sz;
            ++budget.stat_.allocation_count_;
        }
        static inline void AddBugetBlock(RHIMemoryBudget& budget, RHISizeType sz) {
            budget.stat_.block_bytes_ += sz;
            ++budget.stat_.block_count_;
        }
        static inline void RemoveBudgetAllocation(RHIMemoryBudget& budget, RHISizeType sz)
        {
            budget.stat_.allocation_bytes_ -= sz;
            --budget.stat_.allocation_count_;
        }
        static inline void RemoveBudgetBlock(RHIMemoryBudget& budget, RHISizeType sz)
        {
            budget.stat_.block_bytes_ -= sz;
            --budget.stat_.block_count_; 
        }
        static constexpr uint32_t GetDebugMargin() { return MEMORY_DEBUG_MARGIN; }

        BlockMetaHeaderTLSF::BlockMetaHeaderTLSF(RHISizeType size)
        {
            null_block_ = new BlockRange;
            null_block_->offset_ = 0u;
            null_block_->size_ = size;
            
            auto fli = SizeToFirstLevel(size);
            auto sli = SizeToSecondLevel(size, fli);
            list_count_ = fli ? (fli - 1) * (1UL << SECOND_LEVEL_INDEX) + sli + 1UL : 1UL;
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
            alloc_count_ = 0u;
            blocks_free_count_ = 0u;
            blocks_free_size_ = 0u;
            fl_ = 0u;
            memset(sl_, 0u, sizeof(sl_));
            
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

        RHIAllocHandle BlockMetaHeaderTLSF::GetFirstAlloction() const
        {
            if (!alloc_count_) {
                return (RHIAllocHandle)nullptr;
            }
            for (auto* block = null_block_->physic_prev_; block; block = block->physic_prev_)
            {
                if (!block->IsFree())
                {
                    return (RHIAllocHandle)block;
                }
            }
            assert(0);
            return (RHIAllocHandle)nullptr;
        }

        RHIAllocHandle BlockMetaHeaderTLSF::GetNextAllocation(RHIAllocHandle curr) const
        {
            auto* block = reinterpret_cast<BlockRange*>(curr);
            assert(block->IsFree());
            for (block = block->physic_prev_; block; block = block->physic_prev_)
            {
                if (!block->IsFree())
                {
                    return (RHIAllocHandle)block;
                }
            }
            return (RHIAllocHandle)nullptr;
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
                //remove list head
                const auto list_index = GetListIndex(block->size_);
                free_list_head_[list_index] = block->NextFree();
                if (block->NextFree() == nullptr) {
                    const auto fli = SizeToFirstLevel(block->size_);
                    const auto sli = SizeToSecondLevel(block->size_, fli);
                    sl_[fli] &= ~(1U << sli);
                    if (sl_[fli]) {
                        fl &= ~(1UL << fl);
                    }
                }

            }
            block->MarkTaken();
            block->user_data_ = nullptr;
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
                //insert  free list head
                const auto fli = SizeToFirstLevel(block->size_);
                const auto sli = SizeToSecondLevel(block->size_, fli);
                sl_[fli] |= 1U << sli;
                fl_ |= 1UL << fli;
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
            auto inner_free_map = sl_[fli] & (~0U << SizeToSecondLevel(sz, fli));
            if (!inner_free_map) {
                //to try all higher levels
                const auto free_map = fl_ & (~0UL << (fli+1));
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

        bool BlockMetaHeaderTLSF::CheckCorruption() const
        {
            //test malloc memory block magic value
            if (GetDebugMargin() > 0) {
                const auto validate_magic_value = [](const void* block_data, RHISizeType offset) {
                    const auto* ptr = reinterpret_cast<const uint8_t*>(block_data) + offset;
                    return std::all_of(ptr, ptr + GetDebugMargin(), [](auto value) { return value != GetDebugMargin(); });
                };
                while (auto* block = null_block_->physic_prev_)
                {
                    if (!block->IsFree()) {
                        if (!validate_magic_value(mapped_data, block->size_ + block->offset_)) {
                            return false;
                        }
                    }
                }
            }
            //test free list
            for (auto n = 0; n < list_count_; ++n)
            {
                auto* block = free_list_head_[n];
                if (block) {
                    if (!block->IsFree() || block->PrevFree()) {
                        return false;
                    }
                    while (block->NextFree()) {
                        if (!block->NextFree()->IsFree() || block->NextFree()->PrevFree() != block)
                        {
                            return false;
                        }
                        block = block->NextFree();
                    }
                }
            }
            //test physic block range
            if (null_block_->physic_next_) {
                return false;
            }
            if (null_block_->prev_free_ && null_block_->physic_prev_->physic_next_ != null_block_) {
                return false;
            }
            uint32_t calc_alloc_count{ 0u }, calc_free_count{ 0u };
            RHISizeType calc_alloc_size{ 0u }, calc_free_size{ 0u };

            for (auto* block = null_block_->physic_prev_; block != nullptr; block = block->physic_prev_)
            {
                const auto* next_block = block->physic_next_;
                if (next_block && (block->offset_ + block->size_ != next_block->offset_)) //todo alignment
                {
                    return false;
                }
                if (block->IsFree()) {
                    calc_free_size += (block->size_+GetDebugMargin());
                    calc_free_count++;
                    if (block->PrevFree() && block->PrevFree()->NextFree() != block) {
                        return false;
                    }
                    if (block->NextFree() && block->NextFree()->PrevFree() != block) {
                        return false;
                    }
                    //check block in free list
                    const auto list_index = GetListIndex(block->size_);
                    auto* prev_block = block->PrevFree();
                    while (prev_block->PrevFree()) {
                        prev_block = prev_block->PrevFree();
                    }
                    if (prev_block != free_list_head_[list_index]) {
                        return false;
                    }
                }
                else
                {
                    calc_alloc_size += (block->size_+GetDebugMargin());
                    calc_alloc_count++;

                }
            }
            if (calc_alloc_count != alloc_count_ || calc_free_count != blocks_free_count_) {
                return false;
            }
            if (calc_free_size != blocks_free_size_ || calc_free_size + calc_alloc_size != size_) {
                return false;
            }
            return true;
        }
    }

    RHIManagedMemoryBlockVector::RHIManagedMemoryBlockVector(RHIMemoryResidencyManager* const manager, const uint8_t heap_index, const uint32_t min_block_count, const uint32_t max_block_count,
        const RHISizeType preferred_block_size, const RHISizeType expected_block_size, const float priority, EAllocateStrategy algoritm) :is_sort_incremental_(true), manager_(manager), heap_index_(heap_index), min_block_count_(min_block_count), max_block_count_(max_block_count), preferred_block_size_(preferred_block_size), priority_(std::clamp(priority, 0.f, 1.f))
    {
        assert(manager != nullptr && "vector must has one valid parent");
        assert(min_block_count < max_block_count && min_block_count >= 0);
        is_explicit_size_ = !!preferred_block_size_;
        if (preferred_block_size_ == 0u) {
            preferred_block_size_ = GET_PARAM_TYPE_VAL(UINT, RESIDENCY_PREFER_BLOCK_SIZE);
        }
        for (auto n = 0; n < min_block_count_; ++n) {
            RHIManagedMemoryBlock* ptr{ nullptr };
            if (CreateBlock(preferred_block_size_, ptr)) {
                blocks_.emplace_back(ptr);
            }
        }
    }

    void RHIManagedMemoryBlockVector::Alloc(RHISizeType size, RHIAllocation*& memory)
    {

        
    }

    void RHIManagedMemoryBlockVector::PostAlloc(RHIManagedMemoryBlock& block)
    {
        block.map_hysteresis_.PostAlloc();
    }

    void RHIManagedMemoryBlockVector::DeAlloc(RHIAllocation* memory)
    {
        assert(memory != nullptr);
        auto& managed_mem = memory->managed_mem_.mem_;
        auto* block_meta = GetBlockMeta(*managed_mem.parent_);
        block_meta->DeAllocBlock(managed_mem.offset_); //todo

        IncrementalSortBlockByFreeSize();
    }

    void RHIManagedMemoryBlockVector::PostDeAlloc(RHIManagedMemoryBlock& block)
    {
        if (block.map_hysteresis_.PostDeAlloc()) {
            if (!block.map_count_) {
                block.mapped_ = nullptr;
                manager_->UnMapRawMemory(block.rhi_mem_);
            }
        }
    }

    bool RHIManagedMemoryBlockVector::CreateBlock(RHISizeType block_size, RHIManagedMemoryBlock*& block)
    {
        RHIAllocationCreateInfo block_info;
        const auto heap_index = manager_->FindMemoryTypeIndex(block_info);

        //todo
        block = new RHIManagedMemoryBlock;
        block->header_ = new BlockMetaHeaderTLSF(block_size);
        block->rhi_mem_ = manager_->MallocRawMemory(, flags, sz);
        return block != nullptr;
    }

    void RHIManagedMemoryBlockVector::RemoveBlock(RHIManagedMemoryBlock* block)
    {
        std::remove_if(blocks_.begin(), blocks_.end(), [block](const auto* curr_block)->bool {return curr_block == block; });
    }

    void* RHIManagedMemoryBlockVector::MapBlock(RHIManagedMemoryBlock& block)
    {
        /*like d3d12, resource map itself*/
        if (manager_->IsMapInResourceLevel()) {
            return nullptr;
        }
        if ((block.map_count_ + block.map_hysteresis_.GetExtraMapping()) == 0u) {
            manager_->MapRawMemory(block.rhi_mem_);
            block.map_count_ = 0u;
        }
        assert(IsMapped(block) && "block is not mapped");
        ++block.map_count_;
        block.map_hysteresis_.PostMap();
        return block.mapped_;
    }

    void RHIManagedMemoryBlockVector::UnMapBlock(RHIManagedMemoryBlock& block)
    {
        if (manager_->IsMapInResourceLevel()) {
            return;
        }
        --block.map_count_;
        assert(block.map_count_ >= 0 && "being unmapped before mapped");
        if ((block.map_count_+block.map_hysteresis_.GetExtraMapping()) == 0u) {
            manager_->UnMapRawMemory(block.rhi_mem_);
            block.mapped_ = nullptr;
        }
        block.map_hysteresis_.PostUnMap();
    }

    RHISizeType RHIManagedMemoryBlockVector::CalcTotalBlockSize() const
    {
        RHISizeType total_size{ 0u };
        std::for_each(blocks_.cbegin(), blocks_.cend(), [&total_size](auto block) { total_size += GetBlockMeta(*block)->Size(); });
        return total_size;
    }

    RHISizeType RHIManagedMemoryBlockVector::CalcMaxBlockSize() const
    {
        RHISizeType max_size{ 0u };
        for (const auto* block : blocks_) {
            max_size = std::max(max_size, GetBlockMeta(*const_cast<RHIManagedMemoryBlock*>(block))->Size());
            if (max_size >= preferred_block_size_) {
                break;
            }
        }
        return max_size;
    }

    void RHIManagedMemoryBlockVector::IncrementalSortBlockByFreeSize()
    {
        if (is_sort_incremental_) {
            for (auto n = 1; n < blocks_.size(); ++n)
            {
                //only swap the first one
                if (GetBlockMeta(*blocks_[n - 1])->GetSumFreeSize() > GetBlockMeta(*blocks_[n])->GetSumFreeSize()) {
                    std::swap(blocks_[n - 1], blocks_[n]);
                    return;
                }
            }
        }
    }

    void RHIManagedMemoryBlockVector::SortBlocksByFreeSize() {
        std::sort(blocks_.begin(), blocks_.end(), [](const auto* lhs, const auto* rhs) {   
                                                return GetBlockMeta(*const_cast<RHIManagedMemoryBlock*>(lhs))->GetSumFreeSize() >
                                                    GetBlockMeta(*const_cast<RHIManagedMemoryBlock*>(rhs))->GetSumFreeSize(); });
    }

    void RHIManagedMemoryBlockVector::AddStatistics(RHIMemroyStatistics& stats) const
    {
        for (auto* block : blocks_) {
            auto* block_meta = GetBlockMeta(*block);
            block_meta->AddStatistics(stats);
        }
    }

    void RHIManagedMemoryBlockVector::AddDetailedStatistics(RHIMemoryDetailStatistics& stats) const
    {
        for (auto* block : blocks_) {
            auto* block_meta = GetBlockMeta(*block);
            block_meta->AddDetailedStatistics(stats);
        }
    }

    RHIManagedMemoryBlockVector::~RHIManagedMemoryBlockVector()
    {
        for (auto* block : blocks_) {
            assert(block != nullptr);
            //todo destroy block
        }
        blocks_.clear();
    }

    bool RHIManagedMemoryBlockVector::AllocPage(const RHIAllocationCreateInfo& alloc_info, RHISizeType size, RHISizeType alignment, RHIAllocation*& allocation)
    {
        const auto alloc_size = size + GetDebugMargin();
        if (alloc_size > preferred_block_size_) {
            return;
        }
        
        //new allocation memory
        allocation = new RHIAllocation;

        for (auto* block : blocks_) {
            const auto* block_head = GetBlockMeta(*block);
            if (block_head->GetSumFreeRangeCount() > alloc_size) {
                if (AllocFromBlock(*block, alloc_size, 0u, alloc_info.flags_, alloc_info.user_data_.get(), allocation->managed_mem_.mem_))
                {
                    return true;
                }
            }
        }

        const auto vec_budget = manager_->GetMemoryBudget(heap_index_);
        const auto free_memory = (vec_budget.budget_ > vec_budget.usage_) ? (vec_budget.budget_ - vec_budget.usage_) : 0u;
        constexpr auto new_block_size_max_shift{ 3u };

        const auto test_new_block = GetTotalBlockCount() < max_block_count_ && free_memory >= size;
        if (test_new_block) {
            auto new_block_size = preferred_block_size_;
            auto new_block_size_shift = 0u;
            RHIManagedMemoryBlock* curr_block{ nullptr };
            if (!is_explicit_size_) {
                const auto max_existing_block_size = CalcMaxBlockSize();
                for (auto n = 0u; n < new_block_size_max_shift; ++n) {
                    const auto smaller_new_block_size{ new_block_size >> 1 };
                    if (smaller_new_block_size > max_existing_block_size && smaller_new_block_size >= size * 2)
                    {
                        new_block_size = smaller_new_block_size;
                        ++new_block_size_shift;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            auto new_block_index{ 0u };
            if (new_block_size <= free_memory) {
                CreateBlock(new_block_size, curr_block);
            }
            else if (!is_explicit_size_) {
                //try smaller size
                while (new_block_size_shift++ < new_block_size_max_shift)
                {
                    const auto smaller_new_block_size = new_block_size >> 1;
                    if (smaller_new_block_size >= size) {
                        new_block_size = smaller_new_block_size;
                        if (new_block_size < free_memory) {
                            CreateBlock(new_block_size, curr_block);
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }

            if (curr_block) {
                return AllocFromBlock(*curr_block, size, alignment, alloc_info.flags_, alloc_info.user_data_.get(), allocation->managed_mem_.mem_);
            }

        }

        delete allocation;
        allocation = nullptr;
        return false;
    }

    bool RHIManagedMemoryBlockVector::AllocFromBlock(RHIManagedMemoryBlock& block, RHISizeType size, RHISizeType alignment, RHIMemoryFlags flags, void* user_data, RHIManagedMemory& memory)
    {
        AllocationRequest curr_request{};
        if (GetBlockMeta(block)->CreateAllocationRequest(size, alignment, 0, )) {
            if (CommitAllocationRequest(curr_request, block, alignment, flags, user_data, memory)) {
                return true;
            }
        }
        return false;
    }

    bool RHIManagedMemoryBlockVector::CommitAllocationRequest(AllocationRequest& request, RHIManagedMemoryBlock& block, RHISizeType alignment, RHIMemoryFlags flags, void* user_data, RHIManagedMemory& memory)
    {
        const bool mapped = flags & ERHIMemoryFlagBits::ePersistentMapped;
        const bool is_mapping_allowed = flags & ERHIMemoryFlagBits::eMapMak;
        auto* block_meta = GetBlockMeta(block);

        if (mapped) {
            auto res = block_meta->Map(); //todo
            if (!res) {
                return false;
            }
        }
        block_meta->AllocBlock(request, request.size_, user_data); //todo

        IncrementalSortBlockByFreeSize();

        //update budget
        AddBudgetAllocation(manager_->GetMemoryBudget(this->index), request.size_); //todo
        ++manager_->mem_budget_.num_ops_; //todo
        return true;
    }

    RHIDedicatedMemoryBlockList::RHIDedicatedMemoryBlockList(RHIDedicatedMemoryBlockList* const manager, const uint8_t heap_index):manager_(manager), heap_index_(heap_index)
    {
    }

    void RHIDedicatedMemoryBlockList::Alloc(RHISizeType size, RHIDedicatedMemoryBlock*& memory)
    {
    }

    void RHIDedicatedMemoryBlockList::DeAlloc(RHIDedicatedMemoryBlock* memory)
    {
    }

    void* RHIDedicatedMemoryBlockList::MapBlock(RHIDedicatedMemoryBlock& block)
    {
        if (manager_->IsMapInResourceLevel()) {
            return nullptr;
        }

        if (!IsMapped(block)) {
            manager_->MapRawMemory(block.rhi_mem_, 0, block.size_, 0, &block.mapped_);
        }
        return block.mapped_;
    }

    void RHIDedicatedMemoryBlockList::UnMapBlock(RHIDedicatedMemoryBlock& block)
    {
        if (manager_->IsMapInResourceLevel()) {
            return;
        }
        if (IsMapped(block)) {
            manager_->UnMapRawMemory(block.rhi_mem_);
            block.mapped_ = nullptr;
        }
    }

    void RHIDedicatedMemoryBlockList::SortBySize()
    {
    }

    RHIDedicatedMemoryBlockList::~RHIDedicatedMemoryBlockList()
    {
    }

    void RHIDedicatedMemoryBlockList::AddStatistics(RHIMemroyStatistics& stats) const
    {
        auto* block = blocks_;
        while (block != nullptr)
        {
            stats.allocation_bytes_ += block->ptr_->size_;
            ++stats.allocation_count_;
            block = block->next_;
        }
    }

    void RHIDedicatedMemoryBlockList::AddDetailedStatistics(RHIMemoryDetailStatistics& stats) const
    {
        auto* block = blocks_;
        while (block != nullptr)
        {
            stats.stat_.allocation_bytes_ += block->ptr_->size_;
            ++stats.stat_.allocation_count_;
            stats.allocation_max_size_ = std::max(stats.allocation_max_size_, block->ptr_->size_);
            stats.allocation_min_size_ = std::min(stats.allocation_min_size_, block->ptr_->size_);
            block = block->next_;
        }
    }

    RHIAllocation* RHIMemoryResidencyManager::AllocMemory(const RHIAllocationCreateInfo& mem_info)
    {
        if (mem_info.IsDedicated()) {
            return AllocDedicatedMemory(mem_info);
        }
        
        RHIAllocationCreateInfo final_info = mem_info;
        const auto can_alloc_dedicated = true;
        if(can_alloc_dedicated)
        {
            bool preferred_dedicated{ false };
            if (mem_info.Size() > ) {
                preferred_dedicated = true;
            }
            if (mem_budget_.heap_budget_[]) {
                preferred_dedicated = false;
            }
        }

        auto* allocation = AllocManagedMemory(mem_info);
        if (allocation == nullptr && can_alloc_dedicated) {
            //try to allocate dedicated once more
        }
        return allocation;
    }

    RHIAllocation* RHIMemoryResidencyManager::AllocDedicatedMemory(const RHIAllocationCreateInfo& mem_info)
    {
        assert(mem_info.IsDedicated());
        const auto heap_index = FindMemoryTypeIndex(mem_info.type_, mem_info.flags_);

        return nullptr;
    }

    RHIAllocation* RHIMemoryResidencyManager::AllocManagedMemory(const RHIAllocationCreateInfo& mem_info)
    {
        const auto heap_index = FindMemoryTypeIndex(mem_info.type_, mem_info.flags_);
        auto* block_vec = managed_vec_[heap_index];
        return block_vec->Alloc();
    }

    void RHIMemoryResidencyManager::SetBlockVectorSortIncremetal(bool value)
    {
        for (auto& vec : managed_vec_) {
            vec->SetSortIncremetal(value);
        }
    }

    void RHIMemoryResidencyManager::WriteMagicValueToAllocation(RHIAllocation& allocation, RHISizeType offset)
    {
        const auto margin = GetDebugMargin();
        if (!margin) {
            return;
        }
        if (auto* cpu_data = (uint8_t*)Map(allocation)) {
            memset(cpu_data + offset, margin, margin);
            UnMap(allocation);
        }
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

    RHIMemBudget& RHIMemoryResidencyManager::GetMemoryBudget(uint8_t heap_index)
    {
        return mem_budget_.heap_budget_[heap_index];
    }

    RHIMemoryDetailStatistics& RHIMemoryResidencyManager::GetMemoryStatistics(uint8_t heap_index)
    {
        return heap_index == (uint8_t)-1 ? mem_statistics_.total_ : mem_statistics_.heap_details_[heap_index];
    }
  
#if 1 //def USE_MEMORY_DEFRAG
    /*defag need inner struct*/
    namespace
    {
        struct FragmentedBlock
        {
            uint32_t    data_{ 0u };
            RHIManagedMemoryBlock* block_{ nullptr };
        };

        struct StateBalanced
        {
            RHISizeType avg_free_size_{ 0u };
            RHISizeType avg_alloc_size_{ std::numeric_limits<RHISizeType>::max() };
        };

        struct StateExtensive
        {
            enum class EOperation : uint8_t
            {
                eFindFreeBlockBuffer,
                eFindFreeBlockTexture,
                eFindFreeBlockAll,
                eMoveBuffers,
                eMoveTextures,
                eMoveAll,
                eCleanUp,
                eDone,
            };
            EOperation  operation_{ EOperation::eFindFreeBlockBuffer };
            uint32_t    first_free_block_{ std::numeric_limits<uint32_t>::max() };
        };

        struct MoveRangeData
        {
            RHISizeType size_{ 0u };
            RHISizeType alignment_{ 0u };
            DefragMove  move_;
            uint32_t    flags_{ 0u };
        };

        static MoveRangeData GetMoveData(RHIAllocHandle handle, BlockMetaHeaderTLSF& block_meta)
        {
            MoveRangeData move_data{};
            move_data.move_.src_ = (RHIAllocation*)block_meta.GetAllocationPrivateData(handle);
            move_data.size_ = move_data.move_.src_->size_;
            move_data.alignment_ = move_data.move_.src_->alignment_;
            return move_data;
        }

        static void UpdateStateBalanced(RHIManagedMemoryBlockVector& vec, StateBalanced& state)
        {
            state = {};
            uint32_t alloc_count{ 0u }, free_count{ 0u };

            for (auto n = 0u; n < vec.GetTotalBlockCount(); ++n) {
                auto* block_meta = GetBlockMeta(*vec.GetBlock(n));
                alloc_count += block_meta->GetAllocationCount();
                free_count += block_meta->GetSumFreeRangeCount();
                state.avg_free_size_ += block_meta->GetSumFreeSize();
                state.avg_alloc_size_ += block_meta->Size();
                
            }
            state.avg_alloc_size_ = (state.avg_alloc_size_ - state.avg_free_size_) / alloc_count;
            state.avg_free_size_ /= free_count;
        }

        static bool ReAllocWithInBlock(RHIMemoryResidencyDefragExecutor& defrager, RHIManagedMemoryBlockVector& vec,RHIManagedMemoryBlock* block) {
            auto* block_meta = GetBlockMeta(*block);
            for (auto handle = block_meta->GetFirstAlloction(); handle !=(RHIAllocHandle)nullptr; handle = block_meta->GetNextAllocation(handle))
            {
                auto&& move_data = GetMoveData(handle, *block_meta);
                //ignore allocation create by defrager  
                if (GetPrivateData(move_data.move_.src_) == &defrager)
                    continue;
                switch (defrager.TestMoveOp(move_data.move_.src_->size_))
                {
                case EMoveOp::ePass:
                    break;
                case EMoveOp::eIgnore:
                    continue;
                case EMoveOp::eEnd:
                    return true;
                default:
                    assert(0);
                }
                auto offset = move_data.move_.src_;//todo
                if (offset && block_meta->GetSumFreeSize() > move_data.size_) {
                    //realloc in block, and move block forward
                    AllocationRequest request{};
                    if (block_meta->CreateAllocationRequest(move_data.size_, move_data.alignment_, false, ERHIMemoryFlagBits::eStrategyMinOffset, &request))
                    {
                        if (block_meta->GetAllocationOffset(request.alloc_handle_) < offset)
                        {
                            if (vec.Co) {
                                defrager.GetMoves().emplace_back(move_data.move_);
                                if (defrager.UpdatePassStatus(move_data.size_))
                                {
                                    return true;
                                }
                            }

                        }
                    }
                }
            }
            return false;
        }
        static bool AllocInOtherBlock(RHIMemoryResidencyDefragExecutor& defrager, RHIManagedMemoryBlockVector& vec, uint32_t start, uint32_t end, MoveRangeData& data) {
            for (auto test = start; test < end; ++test)
            {
                auto* block = vec.GetBlock(test);
                if (auto* block_meta = GetBlockMeta(*block); block_meta->GetSumFreeSize() >= data.size_) {
                    if (block_meta->AllocFrom()) {
                        defrager.GetMoves().emplace_back(data.move_);
                        if (defrager.UpdatePassStatus(data.size_)) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        static bool MoveDataToFreeBlocks(RHIMemoryResidencyDefragExecutor& defrager, RHIManagedMemoryBlockVector& vec, uint32_t first_free_block, bool& texture_present, bool& buffer_present, bool& other_present) {
            const auto prev_move_count = defrager.GetMovesCount();
            for (auto n = first_free_block; n > 0;)
            {
                auto* block_meta = GetBlockMeta(*vec.GetBlock(--n));
                for (auto handle = block_meta->GetFirstAlloction(); handle != (RHIAllocHandle)nullptr; handle = block_meta->GetNextAllocation(handle))
                {
                    auto&& move_data = GetMoveData(handle, *block_meta);
                    if (GetPrivateData(move_data.move_.src_->managed_mem_.mem_) == &defrager)
                        continue;
                    switch (defrager.TestMoveOp(move_data.size_))
                    {
                    case EMoveOp::ePass:
                        break;
                    case EMoveOp::eIgnore:
                        continue;
                    case EMoveOp::eEnd:
                        return true;
                    default:
                        assert(0);
                    }
                }
            }
            return prev_move_count = defrager.GetMovesCount();
        }

        static bool DoDefragFast(RHIMemoryResidencyDefragExecutor& defrager, RHIManagedMemoryBlockVector& vec, uint32_t index, bool update) {

            for (auto n = vec.GetTotalBlockCount() - 1; n > 0; --n) {
                auto* block_meta = GetBlockMeta(*vec.GetBlock(n));
                for (auto handle = block_meta->GetFirstAlloction(); handle != (RHIAllocHandle)nullptr; handle = block_meta->GetNextAllocation(handle))
                {
                    auto&& move_data = GetMoveData(handle, *block_meta);
                    if (GetPrivateData(move_data.move_.src_) == &defrager)
                        continue;
                    switch (defrager.TestMoveOp(move_data.size_))
                    {
                    case EMoveOp::ePass:
                        break;
                    case EMoveOp::eEnd:
                        return true;
                    case EMoveOp::eIgnore:
                        continue;
                    default:
                        assert(0);
                    }
                    if (AllocInOtherBlock(defrager, vec, 0, n, move_data))
                    {
                        return true;
                    }
                }
            }
            return false;
        }
        static bool DoDefragBalanced(RHIMemoryResidencyDefragExecutor& defrager, RHIManagedMemoryBlockVector& vec, uint32_t index, bool update) {
            auto& vec_state = reinterpret_cast<StateBalanced*>(defrager.GetDefragState())[index];
            if (update && vec_state.avg_alloc_size_ == std::numeric_limits<RHISizeType>::max())
            {
                UpdateStateBalanced(vec, vec_state);
            }
            const auto curr_move_count = defrager.GetMovesCount();
            auto min_free_region_size = vec_state.avg_free_size_ / 2; //todo
            for (auto n = vec.GetTotalBlockCount() - 1; n > 0; --n) {
                auto* block_meta = GetBlockMeta(*vec.GetBlock(n));
                RHISizeType prev_free_region_size{ 0u };
                for (auto handle = block_meta->GetFirstAlloction(); handle != (RHIAllocHandle)nullptr; handle = block_meta->GetNextAllocation(handle))
                {
                    auto&& move_data = GetMoveData(handle, *block_meta);
                    if (GetPrivateData(move_data.move_.src_) == &defrager)
                        continue;
                    switch (defrager.TestMoveOp(move_data.size_))
                    {
                    case EMoveOp::eIgnore:
                        continue;
                    case EMoveOp::ePass:
                        break;
                    case EMoveOp::eEnd:
                        return true;
                    default:
                        assert(0);
                    }
                    const auto prev_move_count = defrager.GetMovesCount();
                    if (AllocInOtherBlock(defrager, vec, 0, n, move_data)) {
                        return true;
                    }
                    const auto next_free_region_size{};
                    auto offset = move_data.move_.src_;//todo
                    //try to realloc in curr block
                    if (prev_move_count == defrager.GetMovesCount() && !offset && block_meta->GetSumFreeSize() >= move_data.size_)
                    {
                        if (prev_free_region_size >= min_free_region_size ||
                            next_free_region_size >= min_free_region_size ||
                            move_data.size_ <= vec_state.avg_alloc_size_ ||
                            move_data.size_ <= vec_state.avg_free_size_)
                        {
                            AllocationRequest request{};
                            if (block_meta->CreateAllocationRequest()) {
                                if (block_meta->GetAllocationOffset(request.alloc_handle_) < offset)
                                {
                                    if (vec.CommitAllocationRequest(request, *vec.GetBlock(n), ) {
                                        defrager.GetMoves().emplace_back(move_data.move_);
                                        if (defrager.UpdatePassStatus(move_data.size_))
                                            return true;
                                    }
                                }
                            }
                        }
                    }
                    prev_free_region_size = next_free_region_size;
                }
            }
            //try once more time
            if (curr_move_count == defrager.GetMovesCount() && !update)
            {
                vec_state.avg_alloc_size_ = std::numeric_limits<RHISizeType>::max();
                return DoDefragBalanced(defrager, vec, index, false); 
            }
            return false;

        }

        static bool DoDefragFull(RHIMemoryResidencyDefragExecutor& defrager, RHIManagedMemoryBlockVector& vec, uint32_t index, bool update) {
            for (auto n = vec.GetTotalBlockCount() - 1; n > 0; --n)
            {
                auto* block_meta = GetBlockMeta(*vec.GetBlock(n));
                for (auto handle = block_meta->GetFirstAlloction(); handle != (RHIAllocHandle)nullptr; handle = block_meta->GetNextAllocation(handle))
                {
                    auto&& move_data = GetMoveData(handle, *block_meta);
                    if (GetPrivateData(move_data.move_.src_) == &defrager)
                        continue;
                    switch (defrager.TestMoveOp(move_data.size_))
                    {
                    case EMoveOp::eIgnore:
                        continue;
                    case EMoveOp::ePass:
                        break;
                    case EMoveOp::eEnd:
                        return true;
                    default:
                        assert(0);
                    }

                    const auto prev_move_count = defrager.GetMovesCount();
                    if (AllocInOtherBlock(defrager, vec, 0, n, move_data))
                        continue;
                    const auto offset = 0;
                    if (prev_move_count == defrager.GetMovesCount() && !offset && block_meta->GetSumFreeSize() > move_data.size_)
                    {
                        AllocationRequest request{};
                        if (block_meta->CreateAllocationRequest()) {
                            if (block_meta->GetAllocationOffset(request.alloc_handle_) < offset)
                            {
                                if (vec.CommitAllocationRequest()) {
                                    defrager.GetMoves().emplace_back(move_data.move_);
                                    if (defrager.UpdatePassStatus(move_data.size_))
                                        return true;
                                }
                            }
                        }

                    }
                }
            }
            return false;
        }
        static bool DoDefragExtensive(RHIMemoryResidencyDefragExecutor& defrager, RHIManagedMemoryBlockVector& vec, uint32_t index, bool update) {
            //todo
            auto& vec_state = reinterpret_cast<StateExtensive*>(defrager.GetDefragState())[index];
            bool texture_present{ false }, buffer_present{ false }, other_present{ false };
            switch (vec_state.operation_)
            {
            case StateExtensive::EOperation::eDone:
                return false;
            case StateExtensive::EOperation::eFindFreeBlockBuffer:
            case StateExtensive::EOperation::eFindFreeBlockTexture:
            case StateExtensive::EOperation::eFindFreeBlockAll:
            {
                if (!vec_state.first_free_block_) {
                    vec_state.operation_ = StateExtensive::EOperation::eCleanUp;
                    return DoDefragFast(defrager, vec, index, update);
                }
                auto last = (vec_state.first_free_block_ == std::numeric_limits<uint32_t>::max() ? vec.GetTotalBlockCount() : vec_state.first_free_block_) - 1u;
                const auto prev_move_count = defrager.GetMovesCount();
                auto* free_meta = GetBlockMeta(*vec.GetBlock(last));
                for (auto handle = free_meta->GetFirstAlloction(); handle != (RHIAllocHandle)nullptr; handle = free_meta->GetNextAllocation(handle))
                {
                    auto&& move_data = GetMoveData(handle, *free_meta);
                    switch (defrager.TestMoveOp(move_data.size_))
                    {
                    case EMoveOp::eIgnore:
                        continue;
                    case EMoveOp::eEnd:
                        return true;
                    case EMoveOp::ePass:
                        break;
                    default:
                        assert(0);
                    }
                    if (AllocInOtherBlock(defrager, vec, 0, last, move_data))
                    {
                        if (prev_move_count != defrager.GetMovesCount() && free_meta->GetNextAllocation(handle) == (RHIAllocHandle)nullptr)
                            vec_state.first_free_block_ = last;
                        return true;
                    }
                }
                if (prev_move_count == defrager.GetMovesCount())
                {
                    while (last > 0) {
                        if (ReAllocWithInBlock(defrager, vec, vec.GetBlock(--last)))
                            return;
                    }
                    if (prev_move_count == defrager.GetMovesCount()) {
                        DoDefragFast(defrager, vec, index, update);
                    }
                }
                else
                {
                    switch (vec_state.operation_)
                    {
                    case StateExtensive::EOperation::eFindFreeBlockBuffer:
                        vec_state.operation_ = StateExtensive::EOperation::eMoveBuffers;
                        break;
                    case StateExtensive::EOperation::eFindFreeBlockTexture:
                        vec_state.operation_ = StateExtensive::EOperation::eMoveTextures;
                        break;
                    case StateExtensive::EOperation::eFindFreeBlockAll:
                        vec_state.operation_ = StateExtensive::EOperation::eMoveAll;
                    default:
                        assert(0);
                        vec_state.operation_ = StateExtensive::EOperation::eMoveTextures;
                    }
                }
                break;
            }
            case StateExtensive::EOperation::eMoveTextures:
            {
                //todo
            }
            case StateExtensive::EOperation::eMoveBuffers:
            {
                //todo
            }
            case StateExtensive::EOperation::eMoveAll:
            {
                //todo
            }
            case StateExtensive::EOperation::eCleanUp:
                // Cleanup is handled below so that other operations may reuse the cleanup code. This case is here to prevent the unhandled enum value warning (C4062).
                break;
            }
            if (vec_state.operation_ == StateExtensive::EOperation::eCleanUp)
            {
                const auto prev_move_count = defrager.GetMovesCount();
                for (auto n = 0; n < vec.GetTotalBlockCount(); ++n)
                {
                    if (ReAllocWithInBlock(defrager, vec, vec.GetBlock(n)))
                        return true;
                }
                if (prev_move_count == defrager.GetMovesCount())
                    vec_state.operation_ = StateExtensive::EOperation::eDone;
            }
            return false;
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
        max_ignore_allocs_count_ = GET_PARAM_TYPE_VAL(UINT, RESIDENCY_DEFRAG_MAX_IGNORE_ALLOCATIONS);
        if (!max_ignore_allocs_count_ || max_ignore_allocs_count_ > 16) {
            max_ignore_allocs_count_ = 16;
        }

        flags_ = GET_PARAM_TYPE_VAL(UINT, RESIDENCY_DEFRAG_STRATEGY);
        switch (flags_ & ERHIDefragFlagBits::eMask) {
        case ERHIDefragFlagBits::eFull:
            defrag_func_ = DoDefragFull;
        case ERHIDefragFlagBits::eExtensive:
        {
            defrag_func_ = DoDefragExtensive;
            const auto state_size = residency_manager_->managed_vec_.size();
            defrag_state_.reset(NewArray<StateExtensive>(state_size), ArrayDeleter<StateExtensive>(state_size));
            break;
        }
        case ERHIDefragFlagBits::eFast:
            defrag_func_ = DoDefragFast;
        case ERHIDefragFlagBits::eBalanced:
        default:
        {
            const auto state_size = residency_manager_->managed_vec_.size();
            defrag_state_.reset(NewArray<StateBalanced>(state_size), ArrayDeleter<StateBalanced>(state_size));
            defrag_func_ = DoDefragBalanced;
            break;
        }
        }

        //set incremental sort false
        for (auto* block : residency_manager_->managed_vec_)
        {
            assert(block != nullptr);
            block->SetSortIncremetal(false);
            block->SortBlocksByFreeSize();
        }
       
    }

    RHIMemoryResidencyDefragExecutor::~RHIMemoryResidencyDefragExecutor()
    {
        //todo set block vec to increment sort ??
        if (residency_manager_) {
            for (auto* block : residency_manager_->managed_vec_) {
                block->SetSortIncremetal(true);
            }
        }
    }

    void RHIMemoryResidencyDefragExecutor::BeginDefragPass(RHIMemoryResidencyManager& manager, DefragMoveBatchInfo& move_info)
    {
        for (auto n = 0; n < manager.managed_vec_.size(); ++n) {

            auto* vec = manager.managed_vec_[n];
            auto to_end = false;
            auto move_offset = moves_.size();
            if (vec->GetTotalBlockCount() > 1u) {
                to_end = defrag_func_(*vec, defrag_state_.get(), n, true); //todo
            }
            else if (vec->GetTotalBlockCount() == 1u) {
                to_end = ReAllocWithInBlock(*this, *vec, vec->GetBlock(0));
            }

            for (; move_offset < moves_.size(); ++move_offset) {
                //to set move dst private data to n
                GetPrivateData(moves_[move_offset].tmp_dst_) = reinterpret_cast<void*>((uintptr_t)n);
            }
            //to do 
            if (to_end) {
                break;
            }
        }
        move_info.count_ = moves_.size();
        move_info.moves_ = move_info.count_ > 0 ? moves_.data() : nullptr;
    }

    void RHIMemoryResidencyDefragExecutor::EndDefragPass(RHIMemoryResidencyManager& manager, DefragMoveBatchInfo& move_info)
    {
        assert(move_info.moves_ != nullptr || move_info.count_ == 0u);
        using FragmentedBlockAllocator = std::remove_pointer_t<decltype(g_rhi_allocator)>::rebind<FragmentedBlock>::other;
        Vector<FragmentedBlock, FragmentedBlockAllocator> immovable_blocks{ *g_rhi_allocator };
        Vector<FragmentedBlock, FragmentedBlockAllocator> mapped_blocks{ *g_rhi_allocator }; //Vulkan use this. D3D12 map by resource

        for (auto n = 0u; n < move_info.count_; ++n) {
            const auto& move = move_info.moves_[n];
            uint32_t prev_count{ 0u }, curr_count{ 0u };
            RHISizeType freed_block_size{ 0u };
            const auto heap_index = 0u; // move.src_. //to do get src allocation heap index
            auto * block_vec = manager.managed_vec_[heap_index];

            //todo
            switch (move.op_)
            {
            case EMoveOp::eCopy:
            {
                //memory map logic
                const auto map_count = 0u; 
                if (!manager.IsMapInResourceLevel() && map_count > 0) {
                    auto* curr_block = GetBlock(*move.src_);
                    auto iter = eastl::find_if(mapped_blocks.begin(), mapped_blocks.end(), [](const auto& iter) { return iter.blocks_ == curr_block; });
                    if (iter == mapped_blocks.end()) {
                        mapped_blocks.emplace_back(FragmentedBlock{ map_count, curr_block });
                    }
                    else
                    {
                        iter->data_ += map_count;
                    }
                }

                {
                    prev_count = block_vec->GetTotalBlockCount();
                    freed_block_size = GetBlockMeta(*GetBlock(*move.tmp_dst_))->Size();
                }
                block_vec->DeAlloc(move.tmp_dst_); //todo 
                {
                    curr_count = block_vec->GetTotalBlockCount();
                }
            }
            break;
            case EMoveOp::eIgnore:
            {
                pass_stats_.freed_bytes_ -= GetSize(*move.src_);
                --pass_stats_.freed_blocks_;
                block_vec->DeAlloc(move.tmp_dst_);
                auto* curr_block = GetBlock(*move.src_);
                auto iter = eastl::find_if(immovable_blocks.cbegin(), immovable_blocks.cend(), [](const auto& iter) { return iter.blocks_ == curr_block});
                if (iter == immovable_blocks.cend()) {
                    immovable_blocks.emplace_back(FragmentedBlock{heap_index, curr_block});
                }
                break;
            }
            break;
            case EMoveOp::eDestroy:
            {
                pass_stats_.freed_bytes_ -= GetSize(*move.src_);
                --pass_stats_.freed_blocks_;
                {
                    prev_count = block_vec->GetTotalBlockCount();
                    freed_block_size = GetBlockMeta(*GetBlock(*move.src_))->Size();
                }
                block_vec->DeAlloc(move.src_);
                {
                    curr_count = block_vec->GetTotalBlockCount();
                }
                RHISizeType dst_tmp_block_size{ 0u };
                {
                    dst_tmp_block_size = GetBlockMeta(*GetBlock(*move.tmp_dst_))->Size();
                }
                block_vec->DeAlloc(move.tmp_dst_);
                {
                    freed_block_size += dst_tmp_block_size * (curr_count - block_vec->GetTotalBlockCount());
                    curr_count = block_vec->GetTotalBlockCount();
                }
                break;
            }
            break;
            default:
                assert(0);
                LOG(ERROR) << "defrag algorithm not support this operation";
            }

            if (auto freed_count = prev_count - curr_count; freed_count > 0) {
                pass_stats_.freed_bytes_ += 0u;
            }

            if ((flags_ & ERHIDefragFlagBits::eExtensive) && defrag_state_.get()) {
                auto& state = reinterpret_cast<StateExtensive*>(defrag_state_.get())[heap_index];
                if (state.first_free_block_ != std::numeric_limits<uint32_t>::max())
                {
                    if (const auto diff = prev_count - curr_count; diff <= state.first_free_block_)
                    {
                        state.first_free_block_ -= diff;
                        if (state.first_free_block_ != 0u) {
                            state.first_free_block_ -= reinterpret_cast<BlockMetaHeaderTLSF*>(block_vec->GetBlock(state.first_free_block_ - 1)->header_)->IsEmpty();
                        }
                    }
                    else
                    {
                        state.first_free_block_ = 0u;
                    }
                }
            }
        }
        move_info.count_ = 0u;
        move_info.moves_ = nullptr;
        moves_.clear();

        global_stats_ += pass_stats_;
        pass_stats_ = {};

        if (immovable_blocks.size()) {

        }

    }

    RHIDefragStatistics RHIMemoryResidencyDefragExecutor::GetDefragPassStatistics() const
    {
        return RHIDefragStatistics();
    }

    EMoveOp RHIMemoryResidencyDefragExecutor::TestMoveOp(RHISizeType size) const
    {
        if (pass_stats_.moved_bytes_ + size > pass_max_bytes_)
        {
            if (++ignore_allocs_ < max_ignore_allocs_count_) {
                return EMoveOp::eIgnore;
            }
            return EMoveOp::eEnd;
        }
        return EMoveOp::eCopy;
    }

    bool RHIMemoryResidencyDefragExecutor::UpdatePassStatus(RHISizeType move_size)
    {
        ++pass_stats_.moved_allocations_;
        pass_stats_.moved_bytes_ += move_size;
        return (pass_stats_.moved_allocations_ >= pass_max_allocations_ || pass_stats_.moved_bytes_ >= pass_max_bytes_);
    }
#endif
}
