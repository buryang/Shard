#include "folly/portability//Builtins.h"
#include "Algorithm.h"

namespace Shard::Utils
{
    static inline size_type ConvertNumToPow2(size_type number) {
        const auto clz = __builtin_clzll(number);
        const auto ceil_pow = 1u << (sizeof(size_type) * 8u - clz);
        const auto floor_pow = 1u << (sizeof(size_type) * 8u - clz - 1u);
        if (ceil_pow - number > number - floor_pow) {
            return std::max(1u, floor_pow);
        }
        return ceil_pow;
    }

    static inline bool IsRingBufferFull(size_type head, size_type tail, size_type capacity) {
        return tail - head >= capacity - 1u;
    }

    static inline bool IsRingBufferEmpty(size_type head, size_type tail, size_type capacity) {
        return tail - head <= 0u;
    }

    template<class T, template<typename> class ContainerType>
    inline TRingBuffer<T, ContainerType>::TRingBuffer(size_type capacity):capacity_(ConvertNumToPow2(capacity))
    {
        storage_.Init(GetCapacity());
    }

    template<class T, template<typename> class ContainerType>
    inline TRingBuffer<T, ContainerType>::TRingBuffer(TRingBuffer&& other):storage_(std::move(other.storage_)), head_(other.head_.load()), tail_(other.tail_.load()), capacity_(other.capacity_)
    {
    }

    template<class T, template<typename> class ContainerType>
    inline void TRingBuffer<T, ContainerType>::operator=(TRingBuffer&& other)
    {
        new (this)TRingBuffer<T, ContainerType>(other);
    }

    template<class T, template<typename> class ContainerType>
    inline bool TRingBuffer<T, ContainerType>::Poll(TRingBuffer<T, ContainerType>::ElementType& el)
    {
        auto tail = tail_.load(std::memory_order::acquire);
        auto head = head_.load(std::memory_order::acquire);
        if (IsRingBufferEmpty(head, tail, capacity_)) {
            return false;
        }
        const auto new_head = (head + 1u);
        if (head_.compare_exchange_weak(head, new_head, std::memory_order::release, std::memory_order::relaxed)) {
            el = storage_.Get(head & (capacity_ - 1u));
            return true;
        }
        return false;//read failed
    }

    template<class T, template<typename> class ContainerType>
    inline bool TRingBuffer<T, ContainerType>::Offer(const ElementType& el)
    {
        auto head = head_.load(std::memory_order::acquire);
        auto tail = tail_.load(std::memory_order::acquire);
        if (IsRingBufferFull(head, tail, capacity_)) {
            return false;
        }
        const auto new_tail = (tail + 1u);
        if (tail_.compare_exchange_weak(tail, new_tail, std::memory_order::release, std::memory_order::relaxed)) {
            storage_.Set(tail & (capacity_ - 1u), el);
            return true;
        }
        return false;
    }

}
