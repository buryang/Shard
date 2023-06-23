#pragma once
#include "Utils/CommonUtils.h"

//https://www.lenshood.dev/2021/04/19/lock-free-ring-buffer/#ring-buffer
namespace Shard
{
	namespace Utils
	{
		template <class T, class StorageClass>
		class TRingBuffer : public RingBuffer
		{
		public:
			using Handle = size_t;
			using ElementType = T;
			TRingBuffer(size_t capacity);
			virtual bool Poll(ElementType& el);
			virtual bool Offer(const ElementType& el);
			size_t	GetCapacity()const {
				return capacity_;
			}
			virtual ~TRingBuffer() {}
		private:
			StorageClass	storage_;
			std::atomic<Handle> head_{ 0u };
			std::atomic<Handle> tail_{ 0u };
			const size_t capacity_;
		};
	}
}

#include "Algorithm.inl"
