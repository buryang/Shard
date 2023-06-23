#include "Memory.h"

namespace Shard
{
	namespace Utils
	{
		void* LinearAllocator::Alloc(std::size_t size)
		{
			assert(offset_ + size < capacity_);
			void* ptr = memory_ + offset_;
			offset_ += size;
			return ptr;
		}

		void LinearAllocator::DeAlloc(void* ptr, std::size_t size)
		{
		}
		
		StackAllocator::StackAllocator(size_t stack_size)
		{
			memory_ = 
		}

		void* StackAllocator::Alloc(std::size_t size)
		{
			assert(offset_ + size < capacity_);
			void* ptr = memory_ + offset_;
			offset_ += size;
			return ptr;
		}

		void StackAllocator::DeAlloc(void* ptr, std::size_t size)
		{
			assert(ptr + size < memory_ + capacity_);
			offset_ = static_cast<std::size_t>(ptr - memory_);
		}
	}
}