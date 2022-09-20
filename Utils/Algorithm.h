#pragma once
#include "Utils/CommonUtils.h"

namespace MetaInit
{
	namespace Utils
	{
		class RingBuffer
		{
		public:
			using Handle = size_t;
			struct Allocation
			{
				Handle	offset_{ 0 };
				size_t	size_{ 0 };
			};
			RingBuffer(size_t capacity) :capacity_(capacity) {}
			Allocation Alloc(size_t size);
			bool Free(Allocation alloc);
			virtual ~RingBuffer() {}
		private:
			Handle head_{ 0 };
			Handle tail_{ 0 };
			SmallVector<Allocation> delete_alloc_;
			const size_t capacity_;
		};

		class LinearBuffer
		{
		public:
			using Handle = size_t;
			struct Allocation
			{
				Handle offset_{ 0 };
				size_t size_{ 0 };
			};
			LinearBuffer(size_t capacity) :capacity_(capacity) {}
			Allocation Alloc(size_t size);
			void Reset() { head_ = 0; }
			virtual ~LinearBuffer() {}
		private:
			Handle head_{ 0 };
			const size_t capacity_;
		};
	}
}

#include "Algorithm.inl"
