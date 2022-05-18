#include "Memory.h"

namespace MetaInit
{
	namespace Utils
	{
		void* Allocator::Alloc(std::size_t size)
		{
			return arena_.allocate(size);
		}
	}
}