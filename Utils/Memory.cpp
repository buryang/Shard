#include "folly/portability//Builtins.h"
#include "Utils/Memory.h"

namespace Shard::Utils
{
	LinearAllocatorImpl::LinearAllocatorImpl(size_type capacity):capacity_(capacity)
	{
		memory_ = ::operator new(capacity_);
	}
	
	void* LinearAllocatorImpl::allocate(size_type size, size_type n)
	{
		void* ptr = reinterpret_cast<uint8_t*>(memory_) + offset_;
		const auto new_offset = offset_ + n * size;
		if (new_offset >= capacity_) {
			return nullptr;
		}
		offset_ = new_offset;
		return ptr;
	}

	void LinearAllocatorImpl::deallocate(void* ptr, size_type n)
	{
		LOG(ERROR) << "linearallocatorimpl has no dealloc implementation";
	}

	void LinearAllocatorImpl::Reset()
	{
		offset_ = 0u;
	}
	LinearAllocatorImpl::~LinearAllocatorImpl()
	{
		::operator delete(memory_);
	}

	StackAllocatorImpl::StackAllocatorImpl(size_type capacity):capactity_(capacity)
	{
		memory_ = ::operator new(capactity_);
	}
	
	void* StackAllocatorImpl::allocate(size_type size, size_type n)
	{
		void* ptr = reinterpret_cast<uint8_t*>(memory_) + offset_;
		const auto new_offset = offset_ + size * n;
		if (new_offset <= capactity_) {
			offset_ = new_offset;
			return ptr;
		}
		return nullptr;
	}
	void StackAllocatorImpl::deallocate(void* ptr, size_type n)
	{
		offset_ = std::uintptr_t(ptr) - std::uintptr_t(memory_);
	}
	void StackAllocatorImpl::Reset()
	{
		offset_ = 0u;
	}
	StackAllocatorImpl::~StackAllocatorImpl()
	{
		::operator delete(memory_);
	}
}

namespace Shard::Utils::MemoryPool
{
#if 0 
	template<bool use_uintptr=true>
	class BinaryMask
	{
	public:
		explicit BinaryMask(std::uintptr_t mask = 0u) :mask_(mask) {}
		explicit BinaryMask(std::uintptr_t mask_ptr, size_type size) :mask_ptr_{ .ptr_ = mask_ptr, .size_ = size } {}
		void SetBitMask(size_type pos) {
			if constexpr (use_uintptr) {
				assert(pos < sizeof(std::uintptr_t));

			}
			else
			{
				auto* binary_mask = reinterpret_cast<uint8_t*>(mask_ptr_->ptr_);
			}
		}
		void SetBitMask(size_type beg_pos, size_type end_pos) {
			if constexpr (use_uintptr) {

			}
		}
		void UnSetBitMask(size_type pos) {
			if constexpr (use_uintptr) {
				assert(pos < sizeof(std::uintptr_t));
			}
		}
		void UnSetBitMask(size_type beg_pos, size_type end_pos) {
			if constexpr (use_uintptr) {

			}
		}
	private:
		union
		{
			std::uintptr_t	mask_{ 0u }; //for mask less than sizeof(std::uintptr_t)
			struct
			{
				std::uintptr_t	ptr_;
				size_type	size_{ 0u };
			}mask_ptr_;
		};
	};
#endif

	Bucket::Bucket(size_type blk_size, size_type blk_count):blk_size_(blk_size), blk_count_(blk_count_)
	{
		const auto bucket_size = blk_size_ * blk_count_;
		memory_ = ::operator new(bucket_size);
		const auto ledger_size = std::ceil(blk_count_ / sizeof(uint8_t));
		ledger_ = ::operator new(ledger_size); //allocation bitmap
		std::memset(memory_, 0, bucket_size);
		std::memset(ledger_, 0, ledger_size);
	}

	Bucket::~Bucket()
	{
		::operator delete(memory_);
		::operator delete(ledger_);
	}

	void* Bucket::allocate(size_type size)
	{
		const auto index = FindContinusBlocks(size);
		if (index != blk_count_) {
			void* ptr = reinterpret_cast<uint8_t*>(memory_) + index;
			const auto n = std::ceil(size / blk_size_);
			SetBlockInUse(index, n);
		}
		return nullptr;
	}

	void Bucket::deallocate(void* ptr, size_type size)
	{
		if (ptr != nullptr) {
			const auto index = (reinterpret_cast<std::uintptr_t>(ptr) - reinterpret_cast<std::uintptr_t>(memory_))/blk_size_;
			const auto n = std::ceil(size / blk_size_);
			SetBlockFree(index, n);
		}
	}

	bool Bucket::IsPointerBelong(void* ptr) const
	{
		auto ptr_diff = reinterpret_cast<std::uintptr_t>(ptr) - reinterpret_cast<std::uintptr_t>(memory_);
		return ptr_diff >= 0u && ptr_diff < blk_size_* blk_count_;
	}

	namespace {
		ALIGN_CACHELINE constexpr uint8_t ledger_mask[]{ 0x0, 0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF };
	}

	//fixme to do check
	size_type Bucket::FindContinusBlocks(size_type size)const
	{
		const auto is_index_suitable = [size, this](const auto index) {
			auto* ledger_ptr = reinterpret_cast<uint8_t*>(ledger_);
			const auto n = uint32_t(std::ceil(size / blk_size_));
			const auto low_index = index >> 3;
			if (!(ledger_ptr[low_index] & (ledger_mask[std::min(8u, n) - index & 0x7] << (index & 0x7)))) {
				return false;
			}
			if (const auto nn = n - (8 - index & 0x7); nn > 0)
			{
				const auto high_index = (nn >> 3) + low_index;
				for (auto curr = low_index + 1; curr < high_index; ++curr) {
					if (ledger_ptr[curr] != 0xFF) {
						return false;
					}
				}
				if (const auto bits = nn & 0x7; bits != 0) {
					if (!(ledger_ptr[high_index] & ledger_mask[bits])) {
						return false;
					}
				}
			}
			return true;
		};
		for (auto n = 0; n < blk_count_; ++n) {
			if (is_index_suitable(n)) {
				return n;
			}
		}
		return blk_count_;
	}

	void Bucket::SetBlockInUse(size_type index, size_type n)
	{
		auto* ledger_ptr = reinterpret_cast<uint8_t*>(ledger_);
		const auto low_index = index >> 3;
		ledger_ptr[low_index] |= (ledger_mask[std::min(8u, uint32_t(n)) - index&0x7] << (index & 0x7));
		if (const auto nn = n - (8 - index & 0x7); nn > 0)
		{
			const auto high_index = (nn >> 3) + low_index;
			for (auto curr = low_index + 1; curr < high_index; ++curr) {
				ledger_ptr[curr] = ledger_mask[8];
			}
			if (const auto bits = nn & 0x7; bits != 0) {
				ledger_ptr[high_index] |= ledger_mask[bits];
			}
		}
	}

	void Bucket::SetBlockFree(size_type index, size_type n)
	{
		auto* ledger_ptr = reinterpret_cast<uint8_t*>(ledger_);
		const auto low_index = index >> 3;
		ledger_ptr[low_index] &= ~(ledger_mask[std::min(8u, uint32_t(n)) - index & 0x7] << (index & 0x7));
		if (const auto nn = n - (0x8 - index & 0x7); nn > 0)
		{
			const auto high_index = (nn >> 3) + low_index;
			for (auto curr = low_index + 1; curr < high_index; ++curr) {
				ledger_ptr[curr] = 0u;
			}
			if (const auto bits = nn & 0x7; bits != 0) {
				ledger_ptr[high_index] &= ~ledger_mask[bits];
			}
		}

	}

	namespace Scalable
	{
		ScalableBucket::ScalableBucket(size_type blk_size) :blk_size_(blk_size), blk_count_(SCALEBLE_CHUNK_SIZE / blk_size)
		{
		}

		static void* GetBlockInChunk(Chunk* chunk, size_type blk_size) {
			--chunk->free_blks_;
			//1.alloc from bump_ptr, may create all free blocks in the
			//create chunk function is too slow 
			if (chunk->bump_ptr_ > (std::uintptr_t)chunk->memory_) {
				chunk->bump_ptr_ -= blk_size;
				return (void*)chunk->bump_ptr_;
			}

			//2.alloc from free list
			if LIKELY(chunk->free_list_) {
				return std::exchange(chunk->free_list_, chunk->free_list_->next_);
			}
			assert(0 &&"cannot reach here");
		}

		static void FreeBlockToChunk(Chunk* chunk, void* ptr)
		{
			++chunk->free_blks_;
			FreeBlock* freed = reinterpret_cast<FreeBlock*>(ptr);
			freed->next_ = std::exchange(chunk->free_list_, freed);
		}

		/*
		* todo: realize a method like https://www.intel.com/content/dam/www/public/us/en/-
		* documents/research/2007-vol11-iss-4-intel-technology-journal.pdf
		*/
		void* ScalableBucket::allocate(size_type size)
		{
			const auto is_chunk_full = [&, this](const Chunk* chunk)->bool {
				return chunk->free_blks_ == 0u;
			};
			
			if (active_chunk_ == nullptr) {
				active_chunk_ = AllocChunk();
			}
			auto* ptr = GetBlockInChunk(active_chunk_, blk_size_);
			if (is_chunk_full(active_chunk_)) {
				TryRecyleExternalFrees(); //before alloc a new chunk, try recyle
				auto* prev_chunk = active_chunk_->prev_;
				if (prev_chunk == nullptr) {
					prev_chunk = AllocChunk();
					prev_chunk->next_ = active_chunk_;
				}
				active_chunk_ = prev_chunk;
			}
			return ptr;
		}

		void ScalableBucket::deallocate(void* ptr, size_type size)
		{
			const auto is_ptr_in_chunk = [ptr](const Chunk* chunk)->bool {
				return ptr >= chunk->memory_ && ((std::uintptr_t)ptr - (std::uintptr_t)chunk->memory_) < SCALEBLE_CHUNK_SIZE;
			};

			const auto is_chunk_empty = [&, this](const Chunk* chunk)->bool {
				return chunk->free_blks_ == blk_count_;
			};

			const auto is_chunk_space_reuse = [&, this](const Chunk* chunk)->bool {
				return chunk->free_blks_ >= uint32_t(blk_count_ * 0.20f);
			};

			//traverse chunk list
			for (auto* chunk = active_chunk_; chunk != nullptr; ) {
				if (is_ptr_in_chunk(chunk))
				{
					FreeBlockToChunk(chunk, ptr);
					if (chunk != active_chunk_) { 
						if (is_chunk_empty(chunk)) {
							if (chunk->prev_) {
								chunk->prev_->next_ = chunk->next_;
							}
							if (chunk->next_) {
								chunk->next_->prev_ = chunk->prev_;
							}
							DeAllocChunk(chunk);
						}
						else
						{
							//move this chunk to actice_chunk's prev
							if (is_chunk_space_reuse(chunk)) {
								if (chunk->prev_) {
									chunk->prev_->next_ = chunk->next_;
								}
								if (chunk->next_) {
									chunk->next_->prev_ = chunk->prev_;
								}
								chunk->prev_ = std::exchange(active_chunk_->prev_, chunk);
								if (chunk->prev_) {
									chunk->prev_->next_ = chunk;
								}
								chunk->next_ = active_chunk_;
							}
						}
					}
					return;
				}
				chunk = chunk->next_;
			}

		}
		void ScalableBucket::deallocate_external(void* ptr, size_type size)
		{
			auto* local_block = reinterpret_cast<FreeBlock*>(ptr);
			FreeBlock* old_block = nullptr;
			do
			{
				_mm_pause();
				old_block = external_freed_.load(std::memory_order::relaxed);
				local_block->next_ = old_block;

			} while (!external_freed_.compare_exchange_weak(old_block, local_block, std::memory_order::release));
		}
		ScalableBucket::~ScalableBucket()
		{
			if (active_chunk_ != nullptr) { 
				for (auto* chunk = active_chunk_->next_; chunk != nullptr; ) {
					auto next = chunk->next_;
					DeAllocChunk(chunk); 
					chunk = next;
				}
				for (auto* chunk = active_chunk_->prev_; chunk != nullptr;) {
					auto prev = chunk->prev_;
					DeAllocChunk(chunk);
					chunk = prev;
				}
				DeAllocChunk(active_chunk_);
			}
		}
		Chunk* ScalableBucket::AllocChunk()
		{
			const void* ptr = ::operator new(blk_size_*blk_count_ + sizeof(Chunk));
			auto* chunk = (Chunk *)ptr;
			chunk->free_blks_ = blk_count_;
			chunk->memory_ = (uint8_t*)ptr + sizeof(Chunk);
			chunk->bump_ptr_ = (std::uintptr_t)chunk->memory_ + blk_size_ * blk_count_;
			return chunk;
		}

		void ScalableBucket::DeAllocChunk(Chunk* chunk)
		{
			::operator delete(chunk);
		}

		void ScalableBucket::RecyleChunks()
		{
		}

		bool ScalableBucket::TryRecyleExternalFrees()
		{
			auto* block = external_freed_.exchange(nullptr, std::memory_order::acquire);
			if (!block) {
				return false;
			}
			while (block)
			{
				deallocate((void*)block, blk_size_);
				block = block->next_;
			}
			return true;
		}

		static inline size_type ConvertNumToPow2(size_type number) {
			const auto clz = 1u << __builtin_clzll(number);
			const auto ceil_pow = 1 << (sizeof(size_t) * 8u - clz + 1u);
			const auto floor_pow = 1 << (sizeof(size_t) * 8u - clz);
			if (ceil_pow - number > number - floor_pow) {
				return floor_pow;
			}
			return ceil_pow;
		}

		void* ScalablePool::allocate(size_type size)
		{
			if (size > PoolBucketInfo<SCALABLE_BUCKET_FAKE_ID>::MaxSize) {
				return ::operator new(size);
			}

			const auto align_size = ConvertNumToPow2(size);
			for (auto& bucket : buckets_) {
				if (bucket.blk_size_ == align_size) {
					return bucket.allocate(size);
				}
			}

		}
		void ScalablePool::deallocate(void* ptr, size_type size)
		{
			if (size > PoolBucketInfo<SCALABLE_BUCKET_FAKE_ID>::MaxSize) {
				::operator delete(ptr);
			}
			const auto align_size = ConvertNumToPow2(size);
			for (auto& bucket : buckets_) {
				if (bucket.blk_size_ == align_size) {
					return bucket.deallocate(ptr, size);
				}
			}
		}
		void ScalablePool::deallocate_external(void* ptr, size_type size)
		{
			if (size > PoolBucketInfo<SCALABLE_BUCKET_FAKE_ID>::MaxSize) {
				::operator delete(ptr);
			}
			const auto align_size = ConvertNumToPow2(size);
			for (auto& bucket : buckets_) {
				if (bucket.blk_size_ == align_size) {
					return bucket.deallocate_external(ptr, size);
				}
			}
		}
	}
}


