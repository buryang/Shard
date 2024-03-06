#include "folly/portability//Builtins.h"
#include "Algorithm.h"
#include <bit>

namespace Shard::Utils
{
    static inline size_type ConvertNumToPow2(size_type number) {
#if 0 
        const auto clz = __builtin_clzll(number);
        const size_type ceil_pow = 1u << (sizeof(size_type) * 8u - clz);
        const size_type floor_pow = 1u << (sizeof(size_type) * 8u - clz - 1u);
#else
        const auto ceil_pow = std::bit_ceil(number);
        const auto floor_pow = std::bit_floor(number);
#endif
        if (ceil_pow - number > number - floor_pow) {
            return std::max(size_type(1u), floor_pow);
        }
        return ceil_pow;
    }

    static inline bool IsRingBufferFull(size_type head, size_type tail, size_type capacity) {
        return tail - head >= capacity - 1u;
    }

    static inline bool IsRingBufferEmpty(size_type head, size_type tail, size_type capacity) {
        return tail - head <= 0u;
    }

    template<class T, T null, template<typename> class ContainerType>
    inline TRingBuffer<T, null, ContainerType>::TRingBuffer(size_type capacity):capacity_(ConvertNumToPow2(capacity))
    {
        storage_.Init(GetCapacity());
    }

    template<class T, T null, template<typename> class ContainerType>
    inline TRingBuffer<T, null, ContainerType>::TRingBuffer(TRingBuffer&& other):storage_(std::move(other.storage_)), head_(other.head_.load()), tail_(other.tail_.load()), capacity_(other.capacity_)
    {
    }

    template<class T, T null, template<typename> class ContainerType>
    inline void TRingBuffer<T, null, ContainerType>::operator=(TRingBuffer&& other)
    {
        new (this)TRingBuffer<T, null, ContainerType>(other);
    }

    template<class T, T null, template<typename> class ContainerType>
    inline bool TRingBuffer<T, null, ContainerType>::TryPoll(TRingBuffer<T, null, ContainerType>::ElementType& el)
    {
        auto tail = tail_.load(std::memory_order::relaxed);
        auto head = head_.load(std::memory_order::acquire);
        if (IsRingBufferEmpty(head, tail, capacity_)) {
            return false;
        }
        const auto new_head = (head + 1u); 
        el = storage_.Get(head & (capacity_ - 1u));
        //MPMC ABA problem
        if (el == null) {
            return false; 
        }
        if (head_.compare_exchange_weak(head, new_head, std::memory_order::release, std::memory_order::relaxed)) {
            storage_.Set(head & (capacity_ - 1u), null);
            return true;
        }
        return false;
    }

    template<class T, T null, template<typename> class ContainerType>
    inline bool TRingBuffer<T, null, ContainerType>::TryOffer(const ElementType& el)
    {
        auto head = head_.load(std::memory_order::relaxed);
        auto tail = tail_.load(std::memory_order::acquire);
        if (IsRingBufferFull(head, tail, capacity_)) {
            return false;
        }
        const auto new_tail = (tail + 1u);
        //fix ABA problem
        auto old_val = storage_.Get(tail & (capacity_ - 1u));
        if (old_val != null) {
            return false;
        }
        if (tail_.compare_exchange_weak(tail, new_tail, std::memory_order::release, std::memory_order::relaxed)) {
            storage_.Set(tail & (capacity_ - 1u), el);
            return true;
        }
        return false;
    }

}
