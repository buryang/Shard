#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Memory.h"

//overload new/del
//more details see http://www.worldcolleges.info/sites/default/files/C%2B%2B_In_Action.pdf Page 369
//if derived class did not override these operation, new call return wrong size(base-class size) 
//warning: std::make_shared use ::new, so cannot use make_shared
#define OVERLOAD_OPERATOR_NEW(class_name) \
public:\
    void* operator new(size_type size){ \
        assert(size == sizeof(class_name)); \
        return g_hal_allocator->allocate(size); \
    } \
    void operator delete(void* ptr, size_type size){ \
        g_hal_allocator->deallocate(ptr, size); \
}

#define OVERLOAD_OPERATOR_NEW_ARRAY(class_name)\
public:
    void* operator new[](size_type size) {\
        assert(size % sizeof(class_name) == 0u); \
        return g_hal_allocator->allocate(size); \
    }\
    void operator delete[](void* ptr, size_type size) {\
        g_hal_allocator->deallocate((uint8_t*)ptr, size);\
    }

namespace Shard::HAL
{
    //global memory alloc for rhi
    extern Shard::Utils::ScalablePoolAllocator<uint8_t>* g_hal_allocator;

    template<typename T>
    FORCE_INLINE T* NewArray(size_type count) {
        return new(g_hal_allocator->allocate(sizeof(T) * count), count)T; //todo
    }

    template<typename T>
    FORCE_INLINE void DeleteArray(T* ptr, size_type count) {
        for (auto n = 0; n < count: ++n)
        {
            (ptr + n)->~T();
        }
        g_hal_allocator->deallocate((uint8_t*)ptr, sizeof(T) * count);
    }

    template<typename T>
    struct ArrayDeleter
    {
        const size_type   count_{ 0u };
        ArrayDeleter(size_type count) noexcept:count_(count) {} //todo fix it
        void operator()(T* ptr)const {
            DeleteArray<T>(ptr, count_);
        }
    };
}
