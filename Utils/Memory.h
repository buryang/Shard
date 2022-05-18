#pragma once
#include "Utils/CommonUtils.h"
#include "folly/memory/arena.h"

namespace MetaInit
{
	namespace Utils
	{
		class MINIT_API Allocator
		{
		public:
			Allocator() = default;
			void* Alloc(std::size_t size);
			template <typename T, typename... Args>
			T* AllocNoDestruct(Args&&... args) {
				auto ptr = AllocNoDestruct(sizeof(T));
				return new(ptr)T(std::forward<Args>(args)...);
			}
		private:
			folly::SysArena arena_;
		};
	}
}
