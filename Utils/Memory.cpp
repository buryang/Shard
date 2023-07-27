#include "Utils/Memory.h"

namespace Shard::Utils
{
	LinearAllocatorImpl::LinearAllocatorImpl(size_type capacity):capacity_(capacity)
	{
		memory_ = ::operator new(capacity_);
	}
	
	void* LinearAllocatorImpl::Alloc(size_type size, size_type n)
	{
		void* ptr = reinterpret_cast<uint8_t*>(memory_) + offset_;
		const auto new_offset = offset_ + n * size;
		if (new_offset >= capacity_) {
			return nullptr;
		}
		offset_ = new_offset;
		return ptr;
	}

	void LinearAllocatorImpl::DeAlloc(void* ptr, size_type n)
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
	
	void* StackAllocatorImpl::Alloc(size_type size, size_type n)
	{
		void* ptr = reinterpret_cast<uint8_t*>(memory_) + offset_;
		const auto new_offset = offset_ + size * n;
		if (new_offset <= capactity_) {
			offset_ = new_offset;
			return ptr;
		}
		return nullptr;
	}
	void StackAllocatorImpl::DeAlloc(void* ptr, size_type n)
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
	Bucket::Bucket(size_type blk_size, size_type blk_count):blk_size_(blk_size), blk_count_(blk_count_)
	{
		const auto bucket_size = blk_size_ * blk_count_;
		memory_ = ::operator new(bucket_size);
		const auto ledger_size = std::ceil(blk_count_ / sizeof(uint8_t));
		ledger_ = ::operator new(ledger_size);
		std::memset(memory_, 0, bucket_size);
		std::memset(ledger_, 0, ledger_size);
	}

	Bucket::~Bucket()
	{
		::operator delete(memory_);
		::operator delete(ledger_);
	}

	void* Bucket::Alloc(size_type size)
	{
		const auto index = FindContinusBlocks(size);
		if (index != blk_count_) {
			void* ptr = reinterpret_cast<uint8_t*>(memory_) + index;
			const auto n = std::ceil(size / blk_size_);
			SetBlockInUse(index, n);
		}
		return nullptr;
	}

	void Bucket::DeAlloc(void* ptr, size_type size)
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
		alignas(128) constexpr uint8_t ledger_mask[]{ 0x0, 0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF };
	}

	//fixme to do check
	size_type Bucket::FindContinusBlocks(size_type size)const
	{
		const auto is_index_suitable = [size, this](const auto index) {
			auto* ledger_ptr = reinterpret_cast<uint8_t*>(ledger_);
			const auto n = std::ceil(size / blk_size_);
			const auto low_index = index >> 3;
			if (!(ledger_ptr[low_index] & (ledger_mask[std::min(8u, uint32_t(n)) - index & 0x7] << (index & 0x7)))) {
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
		ledger_ptr[low_index] |= (ledger_mask[std::min(8u, uint32_t(n)) - index&0x7] << (index & 0x7);
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
		ledger_ptr[low_index] &= ~((ledger_mask[std::min(8u, uint32_t(n)) - index & 0x7] << (index & 0x7));
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
}

