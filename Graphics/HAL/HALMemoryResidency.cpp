#include "HALMemoryResidency.h"
#include <bit>

namespace Shard::HAL
{
    namespace{
        struct BlockRange
        {
            HALSizeType offset_;
            HALSizeType size_;
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
            BlockRange() = default;
            OVERLOAD_OPERATOR_NEW(BlockRange);
        };

        struct BlockRangeIntrusiveLinkedListTraits
        {
            using ItemType = BlockRange;
            static const ItemType* GetPrev(const ItemType* curr) {
                return curr->prev_free_;
            }
            static const ItemType* GetNext(const ItemType* curr) {
                return curr->next_free_;
            }
            static ItemType*& AccessPrev(ItemType* curr) {
                return curr->PrevFree();
            }
            static ItemType*& AccessNext(ItemType* curr) {
                return curr->NextFree();
            }
        };

        struct AllocationRequest
        {
            HALAllocHandle alloc_handle_{ 0u };
            HALSizeType size_{ 0u };
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
            BlockMetaHeaderTLSF(HALSizeType size);
            ~BlockMetaHeaderTLSF();
            bool CreateAllocationRequest(HALSizeType alloc_size, HALSizeType alignment, bool up_address,
                                        uint32_t strategy, AllocationRequest* request);
            void AllocBlock(const AllocationRequest& request, HALSizeType alloc_size, void* private_data);
            void DeAllocBlock(HALAllocHandle memory);
            void Reset();
            uint32_t GetAllocationCount()const;
            HALSizeType Size()const { return size_; }
            HALSizeType GetSumFreeSize()const;
            uint32_t GetSumFreeRangeCount()const;
            HALAllocHandle GetFirstAlloction() const;
            HALAllocHandle GetNextAllocation(HALAllocHandle curr) const;
            bool IsEmpty()const;
            void GetAllocationInfo(HALAllocHandle curr, HALAllocationCreateInfo& info) const;
            HALSizeType GetAllocationOffset(HALAllocHandle handle) const { return ((BlockRange*)handle)->offset_; }
            void*& GetAllocationPrivateData(HALAllocHandle handle) { auto* block = ((BlockRange*)handle); assert(!block->IsFree()); return block->user_data_; }
            void AddStatistics(HALMemroyStatistics& stats) const;
            void AddDetailedStatistics(HALMemoryDetailStatistics& stats) const;

        private:
            DISALLOW_COPY_AND_ASSIGN(BlockMetaHeaderTLSF);
            uint32_t SizeToFirstLevel(HALSizeType sz) const;
            uint32_t SizeToSecondLevel(HALSizeType sz, uint32_t fl) const;
            uint32_t GetListIndex(uint32_t fl, uint32_t sl) const;
            uint32_t GetListIndex(HALSizeType sz) const;

            /*memory block ops*/
            void RemoveFreeBlock(BlockRange* block);
            void InsertFreeBlock(BlockRange* block);
            void MergeBlock(BlockRange* block, BlockRange* prev);
            BlockRange* FindFreeBlock(uint32_t& list_index, HALSizeType sz);
            bool CheckBlock(BlockRange& block, uint32_t list_index, HALSizeType alloc_size, HALSizeType alignment, AllocationRequest* request) const;
            bool CheckCorruption() const;
        private:
            uint32_t    fl_{ 0u };
            uint32_t    sl_[MAX_FIRST_LEVEL_INDEX]{ 0u };

            //data storage
            BlockRange* null_block_{ nullptr };
            using BlockRangeFreeList = Utils::IntrusiveLinkedList<BlockRangeIntrusiveLinkedListTraits>;
            BlockRangeFreeList free_list_[MAX_FREE_LIST_NUM];

            //
            uint32_t list_count_{ 0u };
            HALSizeType size_;
            HALSizeType alloc_count_;
            HALSizeType blocks_free_count_;
            HALSizeType blocks_free_size_;

        };

        static FORCE_INLINE BlockMetaHeaderTLSF* GetBlockMeta(HALManagedMemoryBlock& block) {
            return reinterpret_cast<BlockMetaHeaderTLSF*>(block.header_);
        }

        static constexpr uint32_t GetDebugMargin() { return RESIDENCY_MEMORY_DEBUG_MARGIN; }

        BlockMetaHeaderTLSF::BlockMetaHeaderTLSF(HALSizeType size)
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

        bool BlockMetaHeaderTLSF::CreateAllocationRequest(HALSizeType alloc_size, HALSizeType alignment, bool up_address, uint32_t strategy, AllocationRequest* request)
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
            strategy &= Utils::EnumToInteger(EHALMemoryFlagBits::eStrategyMask);
            strategy = strategy ? strategy : Utils::EnumToInteger(EHALMemoryFlagBits::eStrategyMinTime); //set min time as default

            if (strategy & Utils::EnumToInteger(EHALMemoryFlagBits::eStrategyMinTime)) {
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
            else if (strategy & Utils::EnumToInteger(EHALMemoryFlagBits::eStrategyMinOccupy)) {
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
            else if (strategy & Utils::EnumToInteger(EHALMemoryFlagBits::eStrategyMinOffset)) {
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
                next_list_block = free_list_[next_list_index];
                if (iterate_next_list()) {
                    return true;
                }
            }
            
            //no suitable memory found
            return false;
        }

        void BlockMetaHeaderTLSF::AllocBlock(const AllocationRequest& request, HALSizeType alloc_size, void* private_data)
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

        void BlockMetaHeaderTLSF::DeAllocBlock(HALAllocHandle memory)
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

        HALSizeType BlockMetaHeaderTLSF::GetSumFreeSize() const
        {
            return blocks_free_size_ + null_block_->size_;
        }

        uint32_t BlockMetaHeaderTLSF::GetSumFreeRangeCount() const
        {
            return blocks_free_count_ + 1u; //1 for null block
        }

        HALAllocHandle BlockMetaHeaderTLSF::GetFirstAlloction() const
        {
            if (!alloc_count_) {
                return (HALAllocHandle)nullptr;
            }
            for (auto* block = null_block_->physic_prev_; block; block = block->physic_prev_)
            {
                if (!block->IsFree())
                {
                    return (HALAllocHandle)block;
                }
            }
            assert(0);
            return (HALAllocHandle)nullptr;
        }

        HALAllocHandle BlockMetaHeaderTLSF::GetNextAllocation(HALAllocHandle curr) const
        {
            auto* block = reinterpret_cast<BlockRange*>(curr);
            assert(block->IsFree());
            for (block = block->physic_prev_; block; block = block->physic_prev_)
            {
                if (!block->IsFree())
                {
                    return (HALAllocHandle)block;
                }
            }
            return (HALAllocHandle)nullptr;
        }

        bool BlockMetaHeaderTLSF::IsEmpty() const
        {
            return false;
        }

        void BlockMetaHeaderTLSF::GetAllocationInfo(HALAllocHandle curr, HALAllocationCreateInfo& info) const
        {
            auto curr_block = reinterpret_cast<BlockRange*>(curr);
            //info.user_data_ = curr_block->user_data_;
        }

        void BlockMetaHeaderTLSF::AddStatistics(HALMemroyStatistics& stats) const
        {
            stats.block_count_++;
            stats.block_bytes_ += size_;
            stats.allocation_count_ += alloc_count_;
            stats.allocation_bytes_ += 0u;
        }

        void BlockMetaHeaderTLSF::AddDetailedStatistics(HALMemoryDetailStatistics& stats) const
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

        uint32_t BlockMetaHeaderTLSF::SizeToFirstLevel(HALSizeType sz) const
        {
            if (sz > SMALL_BUFFER_SIZE) {
                return Utils::BitScanMSB(sz) - FIRST_LEVEL_SHITT;
            }
            return 0u;
        }

        uint32_t BlockMetaHeaderTLSF::SizeToSecondLevel(HALSizeType sz, uint32_t fl) const
        {
            if (!fl) {
                return static_cast<uint32_t>(sz >> (fl + FIRST_LEVEL_SHITT)); //todo
            }
        }

        uint32_t BlockMetaHeaderTLSF::GetListIndex(uint32_t fl, uint32_t sl) const
        {
            return fl * SECOND_LEVEL_INDEX + sl;
        }

        uint32_t BlockMetaHeaderTLSF::GetListIndex(HALSizeType sz) const
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

        BlockRange* BlockMetaHeaderTLSF::FindFreeBlock(uint32_t& list_index, HALSizeType sz) 
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
            return free_list_[list_index].pop_front();
        }

        bool BlockMetaHeaderTLSF::CheckBlock(BlockRange& block, uint32_t list_index, HALSizeType alloc_size, HALSizeType alignment, AllocationRequest* request) const
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
                const auto validate_magic_value = [](const void* block_data, HALSizeType offset) {
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
            HALSizeType calc_alloc_size{ 0u }, calc_free_size{ 0u };

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

    HALManagedMemoryBlockVector::HALManagedMemoryBlockVector(HALMemoryResidencyManager* const manager, const HALBlockVectorCreateFlags flags, const uint8_t pool_index, const uint32_t min_block_count, const uint32_t max_block_count,
        const HALSizeType preferred_block_size, const float priority, EAllocateStrategy algoritm, bool multithread) :is_sort_incremental_(true), manager_(manager), pool_index_(pool_index), min_block_count_(min_block_count), max_block_count_(max_block_count), preferred_block_size_(preferred_block_size), priority_(std::clamp(priority, 0.f, 1.f))
    {
        assert(manager != nullptr && "vector must has one valid parent");
        assert(min_block_count < max_block_count && min_block_count >= 0);
        if(multithread) {
            lock_.store(MagicSpinLock::magic_value);
        }
        is_explicit_size_ = !!preferred_block_size_;
        if (preferred_block_size_ == 0u) {
            preferred_block_size_ = GET_PARAM_TYPE_VAL(UINT, RESIDENCY_PREFER_BLOCK_SIZE);
        }
        for (auto n = 0; n < min_block_count_; ++n) {
            HALManagedMemoryBlock* ptr{ nullptr };
            if (CreateBlock(preferred_block_size_, ptr)) {
                blocks_.emplace_back(ptr);
            }
        }
    }

    void HALManagedMemoryBlockVector::Alloc(HALSizeType size, HALAllocation*& memory)
    {

        
    }

    void HALManagedMemoryBlockVector::PostAlloc(HALManagedMemoryBlock& block)
    {
        block.map_hysteresis_.PostAlloc();
    }

    void HALManagedMemoryBlockVector::DeAlloc(HALAllocation* memory)
    {
        assert(memory != nullptr);
        auto& managed_mem = memory->managed_mem_.mem_;
        auto* block_meta = GetBlockMeta(*managed_mem.parent_);
        block_meta->DeAllocBlock(managed_mem.offset_); //todo

        IncrementalSortBlockByFreeSize();
    }

    void HALManagedMemoryBlockVector::PostDeAlloc(HALManagedMemoryBlock& block)
    {
        if (block.map_hysteresis_.PostDeAlloc()) {
            if (!block.map_count_) {
                block.mapped_ = nullptr;
                manager_->UnMapRawMemory(block.rhi_mem_);
            }
        }
    }

    bool HALManagedMemoryBlockVector::CreateBlock(HALSizeType block_size, HALManagedMemoryBlock*& block)
    {
        block = new HALManagedMemoryBlock;
        assert(block != nullptr);
        block->header_ = new BlockMetaHeaderTLSF(block_size);
        block->rhi_mem_ = manager_->MallocRawMemory(pool_index_, block_size, priority_, nullptr);
        if (!block->rhi_mem_) {
            delete block->header_;
            delete block;
            block = nullptr; 
        }
        return block->rhi_mem_ != nullptr;
    }

    void HALManagedMemoryBlockVector::RemoveBlock(HALManagedMemoryBlock* block)
    {
        MagicSpinLock lock(lock_, (std::uintptr_t)this);
        std::remove_if(blocks_.begin(), blocks_.end(), [block](const auto* curr_block)->bool {return curr_block == block; });
    }

    void* HALManagedMemoryBlockVector::MapBlock(HALManagedMemoryBlock& block)
    {
        /*like d3d12, resource map itself*/
        if (manager_->IsMapInResourceLevel()) {
            return nullptr;
        }
        {
            MagicSpinLock lock(lock_, (std::uintptr_t)this);
            if ((block.map_count_ + block.map_hysteresis_.GetExtraMapping()) == 0u) {
                manager_->MapRawMemory(block.rhi_mem_, 0u, 0u, 0u, block.mapped_);
                block.map_count_ = 0u;
            }
            assert(IsMapped(block) && "block is not mapped");
            ++block.map_count_;
            block.map_hysteresis_.PostMap();
        }
        return block.mapped_;
    }

    void HALManagedMemoryBlockVector::UnMapBlock(HALManagedMemoryBlock& block)
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

    HALSizeType HALManagedMemoryBlockVector::CalcTotalBlockSize() const
    {
        HALSizeType total_size{ 0u };
        std::for_each(blocks_.cbegin(), blocks_.cend(), [&total_size](auto block) { total_size += GetBlockMeta(*block)->Size(); });
        return total_size;
    }

    HALSizeType HALManagedMemoryBlockVector::CalcMaxBlockSize() const
    {
        HALSizeType max_size{ 0u };
        for (const auto* block : blocks_) {
            max_size = std::max(max_size, GetBlockMeta(*const_cast<HALManagedMemoryBlock*>(block))->Size());
            if (max_size >= preferred_block_size_) {
                break;
            }
        }
        return max_size;
    }

    void HALManagedMemoryBlockVector::IncrementalSortBlockByFreeSize()
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

    void HALManagedMemoryBlockVector::SortBlocksByFreeSize() {
        std::sort(blocks_.begin(), blocks_.end(), [](const auto* lhs, const auto* rhs) {   
                                                return GetBlockMeta(*const_cast<HALManagedMemoryBlock*>(lhs))->GetSumFreeSize() >
                                                    GetBlockMeta(*const_cast<HALManagedMemoryBlock*>(rhs))->GetSumFreeSize(); });
    }

    void HALManagedMemoryBlockVector::AddStatistics(HALMemroyStatistics& stats) const
    {
        for (auto* block : blocks_) {
            auto* block_meta = GetBlockMeta(*block);
            block_meta->AddStatistics(stats);
        }
    }

    void HALManagedMemoryBlockVector::AddDetailedStatistics(HALMemoryDetailStatistics& stats) const
    {
        for (auto* block : blocks_) {
            auto* block_meta = GetBlockMeta(*block);
            block_meta->AddDetailedStatistics(stats);
        }
    }

    HALManagedMemoryBlockVector::~HALManagedMemoryBlockVector()
    {
        for (auto* block : blocks_) {
            assert(block != nullptr);
            //todo destroy block
        }
        blocks_.clear();
    }

    bool HALManagedMemoryBlockVector::AllocPage(const HALAllocationCreateInfo& alloc_info, HALSizeType size, HALSizeType alignment, HALAllocation*& allocation)
    {
        const auto alloc_size = size + GetDebugMargin();
        if (alloc_size > preferred_block_size_) {
            return;
        }
        
        //new allocation memory
        allocation = new HALAllocation;

        for (auto* block : blocks_) {
            const auto* block_head = GetBlockMeta(*block);
            if (block_head->GetSumFreeRangeCount() > alloc_size) {
                if (AllocFromBlock(*block, alloc_size, 0u, alloc_info.flags_, alloc_info.user_data_.get(), allocation->managed_mem_.mem_))
                {
                    return true;
                }
            }
        }

        const auto vec_budget = manager_->GetMemoryBudget(pool_index_);
        const auto free_memory = (vec_budget.budget_ > vec_budget.usage_) ? (vec_budget.budget_ - vec_budget.usage_) : 0u;
        constexpr auto new_block_size_max_shift{ 3u };

        const auto test_new_block = GetTotalBlockCount() < max_block_count_ && free_memory >= size;
        if (test_new_block) {
            auto new_block_size = preferred_block_size_;
            auto new_block_size_shift = 0u;
            HALManagedMemoryBlock* curr_block{ nullptr };
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

    bool HALManagedMemoryBlockVector::AllocFromBlock(HALManagedMemoryBlock& block, HALSizeType size, HALSizeType alignment, HALMemoryFlags flags, void* user_data, HALManagedMemory& memory)
    {
        AllocationRequest curr_request{};
        if (GetBlockMeta(block)->CreateAllocationRequest(size, alignment, 0, )) {
            if (CommitAllocationRequest(curr_request, block, alignment, flags, user_data, memory)) {
                return true;
            }
        }
        return false;
    }

    bool HALManagedMemoryBlockVector::CommitAllocationRequest(AllocationRequest& request, HALManagedMemoryBlock& block, HALSizeType alignment, HALMemoryFlags flags, void* user_data, HALManagedMemory& memory)
    {
        const bool mapped = flags & EHALMemoryFlagBits::ePersistentMapped;
        const bool is_mapping_allowed = flags & EHALMemoryFlagBits::eMapMak;
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
        //AddBudgetAllocation(manager_->GetMemoryBudget(this->pool_index_), request.size_); //todo
        //++manager_->mem_budget_.num_ops_; //todo
        return true;
    }

    HALDedicatedMemoryBlockList::HALDedicatedMemoryBlockList(HALMemoryResidencyManager* const manager, const uint8_t pool_index, const float priority, bool multithread):manager_(manager), pool_index_(pool_index), priority_(priority)
    {
        if (multithread) {
            lock_.store(MagicSpinLock::magic_value);
        }
    }

    void HALDedicatedMemoryBlockList::Alloc(HALSizeType size, HALAllocation*& memory)
    {
        MagicSpinLock lock(lock_, (uintptr_t)this);
        auto iter = std::find_if(free_blocks_.begin(), free_blocks_.end(), [size](auto iter) { return iter.dedicated_mem_.size_ > size; });
        if (iter != free_blocks_.end()) {
            memory = std::addressof(*iter);
            free_blocks_.erase(iter);
            return;
        }
        
        //initial one new allocation
        memory = new HALAllocation;
        memory->is_dedicated_ = 1;
        memory->dedicated_mem_ = {};
        memory->dedicated_mem_.size_ = size;
        memory->dedicated_mem_.rhi_mem_ = manager_->MallocRawMemory(pool_index_, size, 1);
        memory->pool_index_ = pool_index_;
    }

    void HALDedicatedMemoryBlockList::DeAlloc(HALAllocation* memory)
    {
        MagicSpinLock lock(lock_, (uintptr_t)this);
        //todo sort?
        free_blocks_.push_front(memory);
    }

    void HALDedicatedMemoryBlockList::SortBySize()
    {
        std::sort(free_blocks_.begin(), free_blocks_.end(), [](auto lhs, auto rhs) { return GetSize(*lhs) > GetSize(*rhs); });
    }

    HALDedicatedMemoryBlockList::~HALDedicatedMemoryBlockList()
    {
        for (auto iter = free_blocks_.begin(); iter != free_blocks_.end(); ++iter) {
            delete std::addressof(*iter); 
        }
    }

    void HALDedicatedMemoryBlockList::AddStatistics(HALMemroyStatistics& stats) const
    {
        for(auto iter = free_blocks_.cbegin(); iter != free_blocks_.cend(); ++iter)
        {
            stats.allocation_bytes_ += GetSize(*iter);
            ++stats.allocation_count_;
        }
    }

    void HALDedicatedMemoryBlockList::AddDetailedStatistics(HALMemoryDetailStatistics& stats) const
    {
        for (auto iter = free_blocks_.cbegin(); iter != free_blocks_.cend(); ++iter)
        {
            stats.stat_.allocation_bytes_ += GetSize(*iter);
            ++stats.stat_.allocation_count_;
            stats.allocation_max_size_ = std::max(stats.allocation_max_size_, GetSize(*iter));
            stats.allocation_min_size_ = std::min(stats.allocation_min_size_, GetSize(*iter));
        }
    }

    HALAllocation* HALMemoryResidencyManager::AllocMemory(const HALAllocationCreateInfo& mem_info)
    {
        if (mem_info.IsDedicated()) {
            return AllocDedicatedMemory(mem_info);
        }
        
        HALAllocationCreateInfo final_info = mem_info;
        const auto pool_index = FindMemoryPoolIndex(final_info.type_, final_info.flags_, final_info.user_data_.get());
        const auto can_alloc_dedicated = !managed_vec_[pool_index]->IsExplicitSize();
        if(can_alloc_dedicated)
        {
            bool preferred_dedicated{ false };
            if (mem_info.Size() > managed_vec_[pool_index]->GetPreferredBlockSize()*0.5f) {
                preferred_dedicated = true;
            }
            if (GetMaxAllocationCount() < std::numeric_limits<uint32_t>::max()/4 && mem_budget_.alloc_count_ > GetMaxAllocationCount()*3/4) {
                preferred_dedicated = false;
            }
            if (preferred_dedicated) {
                auto* allocation = AllocDedicatedMemory(final_info);
                if (allocation) {
                    return allocation;
                }
            }
        }

        return AllocManagedMemory(mem_info);
    }

    HALAllocation* HALMemoryResidencyManager::AllocMemoryBatch(const HALAllocationCreateInfo& mem_info, uint32_t count)
    {
        assert(0);//not implement
    }

    HALAllocation* HALMemoryResidencyManager::AllocDedicatedMemory(const HALAllocationCreateInfo& mem_info)
    {
        assert(mem_info.IsDedicated());
        const auto pool_index = FindMemoryPoolIndex(mem_info.type_, mem_info.flags_);
        
        HALAllocation* allocation = nullptr;
        dedicated_vec_[pool_index]->Alloc(mem_info.size_, allocation);
        if (allocation != nullptr) {
            if (!IsMapInResourceLevel() && (mem_info.flags_ & EHALMemoryFlagBits::ePersistentMapped)) {
                Map(nullptr, allocation);
            }
            else
            {
                allocation->is_map_allowed_ = !!(mem_info.flags_ & EHALMemoryFlagBits::eAllowMapped);
            }
        }
        return allocation;
    }

    HALAllocation* HALMemoryResidencyManager::AllocManagedMemory(const HALAllocationCreateInfo& mem_info)
    {
        const auto pool_index = FindMemoryPoolIndex(mem_info.type_, mem_info.flags_);
        auto* block_vec = managed_vec_[pool_index];
        HALAllocation* allocation = nullptr;
        block_vec->Alloc(mem_info.size_, allocation);
        if (allocation != nullptr) {
            if (!IsMapInResourceLevel() && (mem_info.flags_ & EHALMemoryFlagBits::ePersistentMapped)) {
                Map(nullptr, allocation);
            }
            else
            {
                allocation->is_map_allowed_ = !!(mem_info.flags_ & EHALMemoryFlagBits::eAllowMapped);
            }
        }
        return allocation;
    }

    void HALMemoryResidencyManager::DeAllocMemory(HALAllocation* allocation)
    {
        assert(allocation != nullptr);
        const auto pool_index = allocation->pool_index_;
        if (IsDedicated(*allocation)) {
            dedicated_vec_[pool_index]->DeAlloc(allocation);
        }
        else
        {
            managed_vec_[pool_index]->DeAlloc(allocation);
        }
    }

    void HALMemoryResidencyManager::SetBlockVectorSortIncremental(bool value)
    {
        for (auto& vec : managed_vec_) {
            vec->SetSortIncremental(value);
        }
    }

    void HALMemoryResidencyManager::TurnOnPoolBudgetLimit(uint8_t pool_index, HALSizeType limit_size)
    {
        budget_limit_mask_ &= (1u << pool_index);
        pool_budget_limit_[pool_index] = std::min(limit_size, mem_budget_.pool_budget_[pool_index].budget_); //budget dynamic refresh every frame
    }

    void HALMemoryResidencyManager::TurnOffPoolBudgetLimit(uint8_t pool_index)
    {
        budget_limit_mask_ &= ~(1u << pool_index);
    }

#if RESIDENCY_MEMORY_DEBUG_MARGIN
    void HALMemoryResidencyManager::WriteMagicValueToAllocation(HALAllocation& allocation, HALSizeType offset)
    {
        const auto margin = GetDebugMargin();
        if (!margin) {
            return;
        }
        if (auto* cpu_data = (uint8_t*)Map(nullptr, &allocation)) {
            memset(cpu_data + offset, margin, margin);
            UnMap(nullptr, &allocation);
        }
    }

    bool HALMemoryResidencyManager::VerifyAllocationMagicValue(HALAllocation& allocation, HALSizeType offset) 
    {
        const auto margin = GetDebugMargin();
        bool result{ true };
        if (margin) {
            if (auto* cpu_data = (uint8_t*)Map(nullptr, &allocation)) {
                cpu_data += offset;
                for (auto n = 0; n < margin; ++n) {
                    if (*cpu_data++ != margin) {
                        result = false;
                        break;
                    }
                }
                UnMap(nullptr, &allocation);
            }
        }
        return false;
    }
    void HALMemoryResidencyManager::GetAllocationInfo(HALAllocation& allocation, HALAllocationCreateInfo& info)
    {
        if (IsDedicated(allocation)) {
            info.size_ = allocation.dedicated_mem_.size_;
            info.size_ |= HALAllocationCreateInfo::DEDICATE_FLAGS_BIT;
            info.alignment_ = 0u;
        }
        else
        {
            info.size_ = allocation.managed_mem_.mem_.size_;
            GetBlockMeta(*GetParent(allocation.managed_mem_))->GetAllocationInfo(allocation.managed_mem_.mem_, info);
            info.alignment_ = 0u;
        }
    }
#endif

#ifdef RESIDENCY_PRINT_ALLOC_INFO
    void HALMemoryResidencyManager::OutputMemoryAllocationInfos() const
    {
        //todo 
    }
#endif

    void HALMemoryResidencyManager::Tick(float time)
    {
        curr_time_stamp_ = time;
        curr_frame_index_++; //todo 
        UpdateMemoryBudget();
    }

    void HALMemoryResidencyManager::MakeBufferResident(HALBuffer* buffer_ptr, HALAllocation& allocation, HALSizeType offset)
    {
        if (IsDedicated(allocation)) {
            assert(offset == 0 && "dedicated memory should bind without offset");
            MakeBufferResidentImpl(buffer_ptr, allocation.dedicated_mem_.rhi_mem_, 0u);
        }
        else
        {
            MakeBufferResidentImpl(buffer_ptr, GetBlock(allocation)->rhi_mem_, offset + allocation.managed_mem_.offset_);
        }
    }

    void HALMemoryResidencyManager::MakeTextureResident(HALTexture* texture_ptr, HALAllocation& allocation, HALSizeType offset)
    {
        if (IsDedicated(allocation)) {
            assert(offset == 0 && "dedicated memory should bind without offset");
            MakeTextureResidentImpl(texture_ptr, allocation.dedicated_mem_.rhi_mem_, 0u);
        }
        else
        {
            MakeTextureResidentImpl(texture_ptr, GetBlock(allocation)->rhi_mem_, offset + allocation.managed_mem_.offset_);
        }
    }

    HALMemoryBudget& HALMemoryResidencyManager::GetMemoryBudget(uint8_t pool_index)
    {
        return mem_budget_.pool_budget_[pool_index];
    }

    HALMemoryTotalBudget& HALMemoryResidencyManager::GetTotalMemoryBudget()
    {
        return mem_budget_;
    }

    HALMemoryDetailStatistics& HALMemoryResidencyManager::GetMemoryStatistics(uint8_t pool_index)
    {
        return pool_index == (uint8_t)-1 ? mem_statistics_.total_ : mem_statistics_.heap_details_[pool_index];
    }

    HALMemoryTotalStatistics& HALMemoryResidencyManager::GetTotalMemoryStatistics()
    {
        return mem_statistics_;
    }

    void HALMemoryResidencyManager::CalcMemoryStatistics(uint8_t pool_index)
    {
        auto& pool_stats = mem_statistics_.heap_details_[pool_index];
        managed_vec_[pool_index]->AddDetailedStatistics(pool_stats);
        dedicated_vec_[pool_index]->AddDetailedStatistics(pool_stats);
    }

    void HALMemoryResidencyManager::CalcTotalMemoryStatistics()
    {
        for (auto n = 0; n < GetMemoryPoolCount(); ++n) {
            CalcMemoryStatistics(n);
            mem_statistics_.total_ += mem_statistics_.heap_details_[n];
        }
    }

    bool HALMemoryResidencyManager::AccquireMemoryBudgetAllocation(uint8_t pool_index, HALSizeType size)
    {
        mem_budget_.pool_budget_[pool_index].allocation_bytes_.fetch_add(size, std::memory_order::release);
        mem_budget_.pool_budget_[pool_index].allocation_count_.fetch_add(1u);
        mem_budget_.num_ops_.fetch_add(1u);
        return true;
    }

    bool HALMemoryResidencyManager::AccquireMemoryBudgetBlock(uint8_t pool_index, HALSizeType size)
    {
        if (budget_limit_mask_ & (1u << pool_index)) {

            do
            {
                auto old_alloc_size = mem_budget_.pool_budget_[pool_index].block_bytes_.load();
                auto new_alloc_size = old_alloc_size + size;
                if (new_alloc_size > mem_budget_.pool_budget_[pool_index].budget_) {
                    return false;
                }

                if (mem_budget_.pool_budget_[pool_index].block_bytes_.compare_exchange_weak(old_alloc_size, new_alloc_size, std::memory_order::release)) {
                    break;
                }
                _mm_pause();
            } while (true);

        }
        else
        {
            mem_budget_.pool_budget_[pool_index].block_bytes_.fetch_add(size, std::memory_order::release);
        }
        mem_budget_.pool_budget_[pool_index].block_count_.fetch_add(1u);
        mem_budget_.alloc_count_.fetch_add(1u);
        mem_budget_.num_ops_.fetch_add(1u);
        return true;
    }

    void HALMemoryResidencyManager::ReleaseMemoryBudgetAllocation(uint8_t pool_index, HALSizeType size)
    {
        mem_budget_.pool_budget_[pool_index].allocation_bytes_.fetch_sub(size, std::memory_order::acquire);
        mem_budget_.pool_budget_[pool_index].allocation_count_.fetch_sub(1u);
        mem_budget_.alloc_count_.fetch_sub(1u);
        mem_budget_.num_ops_.fetch_add(1u);
    }

    void HALMemoryResidencyManager::ReleaseMemoryBudgetBlock(uint8_t pool_index, HALSizeType size)
    {
        mem_budget_.pool_budget_[pool_index].block_bytes_.fetch_sub(size, std::memory_order::acquire);
        mem_budget_.pool_budget_[pool_index].block_count_.fetch_sub(1u);
        mem_budget_.num_ops_.fetch_add(1u);
    }
  
#ifdef RESIDENCY_MEMORY_DEFRAG
    /*defag need inner struct*/
    namespace
    {
        struct FragmentedBlock
        {
            uint32_t    data_{ 0u };
            HALManagedMemoryBlock* block_{ nullptr };
        };

        struct StateBalanced
        {
            HALSizeType avg_free_size_{ 0u };
            HALSizeType avg_alloc_size_{ std::numeric_limits<HALSizeType>::max() };
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
            HALSizeType size_{ 0u };
            HALSizeType alignment_{ 0u };
            DefragMove  move_;
            uint32_t    flags_{ 0u };
        };

        static MoveRangeData GetMoveData(HALAllocHandle handle, BlockMetaHeaderTLSF& block_meta)
        {
            MoveRangeData move_data{};
            move_data.move_.src_ = (HALAllocation*)block_meta.GetAllocationPrivateData(handle);
            move_data.size_ = move_data.move_.src_->managed_mem_.mem_.size_;
            move_data.alignment_ = move_data.move_.src_->managed_mem_.mem_.alignment_;
            return move_data;
        }

        static void UpdateStateBalanced(HALManagedMemoryBlockVector& vec, StateBalanced& state)
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

        static bool ReAllocWithInBlock(HALMemoryResidencyDefragExecutor& defrager, HALManagedMemoryBlockVector& vec,HALManagedMemoryBlock* block) {
            auto* block_meta = GetBlockMeta(*block);
            for (auto handle = block_meta->GetFirstAlloction(); handle !=(HALAllocHandle)nullptr; handle = block_meta->GetNextAllocation(handle))
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
                    if (block_meta->CreateAllocationRequest(move_data.size_, move_data.alignment_, false, EHALMemoryFlagBits::eStrategyMinOffset, &request))
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
        static bool AllocInOtherBlock(HALMemoryResidencyDefragExecutor& defrager, HALManagedMemoryBlockVector& vec, uint32_t start, uint32_t end, MoveRangeData& data) {
            for (auto test = start; test < end; ++test)
            {
                auto* block = vec.GetBlock(test);
                if (auto* block_meta = GetBlockMeta(*block); block_meta->GetSumFreeSize() >= data.size_) {
                    if (block_meta->Alloc()) {
                        defrager.GetMoves().emplace_back(data.move_);
                        if (defrager.UpdatePassStatus(data.size_)) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        static bool MoveDataToFreeBlocks(HALMemoryResidencyDefragExecutor& defrager, HALManagedMemoryBlockVector& vec, uint32_t first_free_block, bool& texture_present, bool& buffer_present, bool& other_present) {
            const auto prev_move_count = defrager.GetMovesCount();
            for (auto n = first_free_block; n > 0;)
            {
                auto* block_meta = GetBlockMeta(*vec.GetBlock(--n));
                for (auto handle = block_meta->GetFirstAlloction(); handle != (HALAllocHandle)nullptr; handle = block_meta->GetNextAllocation(handle))
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

        static bool DoDefragFast(HALMemoryResidencyDefragExecutor& defrager, HALManagedMemoryBlockVector& vec, uint32_t index, bool update) {

            for (auto n = vec.GetTotalBlockCount() - 1; n > 0; --n) {
                auto* block_meta = GetBlockMeta(*vec.GetBlock(n));
                for (auto handle = block_meta->GetFirstAlloction(); handle != (HALAllocHandle)nullptr; handle = block_meta->GetNextAllocation(handle))
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
        static bool DoDefragBalanced(HALMemoryResidencyDefragExecutor& defrager, HALManagedMemoryBlockVector& vec, uint32_t index, bool update) {
            auto& vec_state = reinterpret_cast<StateBalanced*>(defrager.GetDefragState())[index];
            if (update && vec_state.avg_alloc_size_ == std::numeric_limits<HALSizeType>::max())
            {
                UpdateStateBalanced(vec, vec_state);
            }
            const auto curr_move_count = defrager.GetMovesCount();
            auto min_free_region_size = vec_state.avg_free_size_ / 2; //todo
            for (auto n = vec.GetTotalBlockCount() - 1; n > 0; --n) {
                auto* block_meta = GetBlockMeta(*vec.GetBlock(n));
                HALSizeType prev_free_region_size{ 0u };
                for (auto handle = block_meta->GetFirstAlloction(); handle != (HALAllocHandle)nullptr; handle = block_meta->GetNextAllocation(handle))
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
                vec_state.avg_alloc_size_ = std::numeric_limits<HALSizeType>::max();
                return DoDefragBalanced(defrager, vec, index, false); 
            }
            return false;

        }

        static bool DoDefragFull(HALMemoryResidencyDefragExecutor& defrager, HALManagedMemoryBlockVector& vec, uint32_t index, bool update) {
            for (auto n = vec.GetTotalBlockCount() - 1; n > 0; --n)
            {
                auto* block_meta = GetBlockMeta(*vec.GetBlock(n));
                for (auto handle = block_meta->GetFirstAlloction(); handle != (HALAllocHandle)nullptr; handle = block_meta->GetNextAllocation(handle))
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
        static bool DoDefragExtensive(HALMemoryResidencyDefragExecutor& defrager, HALManagedMemoryBlockVector& vec, uint32_t index, bool update) {
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
                for (auto handle = free_meta->GetFirstAlloction(); handle != (HALAllocHandle)nullptr; handle = free_meta->GetNextAllocation(handle))
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
                        if (prev_move_count != defrager.GetMovesCount() && free_meta->GetNextAllocation(handle) == (HALAllocHandle)nullptr)
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

    HALMemoryResidencyDefragExecutor::HALMemoryResidencyDefragExecutor(HALMemoryResidencyManager* manager):residency_manager_(manager)
    {
        assert(manager != nullptr);
        pass_max_bytes_ = GET_PARAM_TYPE_VAL(UINT, RESIDENCY_DEFRAG_MAX_BYTES);
        if (!pass_max_bytes_) {
            pass_max_bytes_ = std::numeric_limits<HALSizeType>::max();
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
        switch (flags_ & EHALDefragFlagBits::eMask) {
        case EHALDefragFlagBits::eFull:
            defrag_func_ = DoDefragFull;
        case EHALDefragFlagBits::eExtensive:
        {
            defrag_func_ = DoDefragExtensive;
            const auto state_size = residency_manager_->managed_vec_.size();
            defrag_state_.reset(NewArray<StateExtensive>(state_size), ArrayDeleter<StateExtensive>(state_size));
            break;
        }
        case EHALDefragFlagBits::eFast:
            defrag_func_ = DoDefragFast;
        case EHALDefragFlagBits::eBalanced:
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
            block->SetSortIncremental(false);
            block->SortBlocksByFreeSize();
        }
       
    }

    HALMemoryResidencyDefragExecutor::~HALMemoryResidencyDefragExecutor()
    {
        //todo set block vec to increment sort ??
        if (residency_manager_) {
            for (auto* block : residency_manager_->managed_vec_) {
                block->SetSortIncremental(true);
            }
        }
    }

    void HALMemoryResidencyDefragExecutor::BeginDefragPass(HALMemoryResidencyManager& manager, DefragMoveBatchInfo& move_info)
    {
        for (auto n = 0; n < manager.managed_vec_.size(); ++n) {

            auto* vec = manager.managed_vec_[n];
            auto to_end = false;
            auto move_offset = moves_.size();
            if (vec->GetTotalBlockCount() > 1u) {
                to_end = defrag_func_(*this, *vec, n, true); //todo
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

    void HALMemoryResidencyDefragExecutor::EndDefragPass(HALMemoryResidencyManager& manager, DefragMoveBatchInfo& move_info)
    {
        assert(move_info.moves_ != nullptr || move_info.count_ == 0u);
        using FragmentedBlockAllocator = std::remove_pointer_t<decltype(g_hal_allocator)>::rebind<FragmentedBlock>::other;
        Vector<FragmentedBlock, REBIND_ALLOCATOR()> immovable_blocks{*g_hal_allocator};
        Vector<FragmentedBlock, FragmentedBlockAllocator> mapped_blocks{ *g_hal_allocator }; //Vulkan use this. D3D12 map by resource

        for (auto n = 0u; n < move_info.count_; ++n) {
            const auto& move = move_info.moves_[n];
            uint32_t prev_count{ 0u }, curr_count{ 0u };
            HALSizeType freed_block_size{ 0u };
            const auto pool_index = 0u; // move.src_. //to do get src allocation heap index
            auto * block_vec = manager.managed_vec_[pool_index];

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
                    immovable_blocks.emplace_back(FragmentedBlock{pool_index, curr_block});
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
                HALSizeType dst_tmp_block_size{ 0u };
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

            if ((flags_ & EHALDefragFlagBits::eExtensive) && defrag_state_.get()) {
                auto& state = reinterpret_cast<StateExtensive*>(defrag_state_.get())[pool_index];
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

    HALDefragStatistics HALMemoryResidencyDefragExecutor::GetDefragPassStatistics() const
    {
        return HALDefragStatistics();
    }

    EMoveOp HALMemoryResidencyDefragExecutor::TestMoveOp(HALSizeType size) const
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

    bool HALMemoryResidencyDefragExecutor::UpdatePassStatus(HALSizeType move_size)
    {
        ++pass_stats_.moved_allocations_;
        pass_stats_.moved_bytes_ += move_size;
        return (pass_stats_.moved_allocations_ >= pass_max_allocations_ || pass_stats_.moved_bytes_ >= pass_max_bytes_);
    }
#endif
}
