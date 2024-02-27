#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Memory.h"

//global memory alloc for rhi
extern class Shard::Utils::ScalablePoolAllocator<uint8_t>* g_rhi_allocator;
//overload new/del
//more details see http://www.worldcolleges.info/sites/default/files/C%2B%2B_In_Action.pdf Page 369
//if derived class did not override these operation, new call return wrong size(base-class size) 
//warning: std::make_shared use ::new, so cannot use make_shared
#define OVERLOAD_OPERATOR_NEW(class_name) \
public:\
    void* operator new(std::size_t size){ \
        assert(size == sizeof(class_name)); \
        return g_rhi_allocator->Alloc(size); \
    } \
    void operator delete(void* ptr, std::size_t size){ \
        g_rhi_allocator->DeAlloc(ptr, size); \
}