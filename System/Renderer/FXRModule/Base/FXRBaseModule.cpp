#include "FXRRenderBase.h"

namespace Shard::Renderer::FXR
{
    void* ViewLocalAllocator::allocate(size_type size, size_type n)
    {
        const auto alloc_size = size * n;
        if (alloc_size + offset_ >= capacity_) {
            LOG(ERROR) << "view local allocator overflow";
            return nullptr;
        }
        void* memory = reinterpret_cast<uint8_t*>(bulk_ptr_) + offset_;
        offset_ += alloc_size;
        return memory;
    }

    void ViewLocalAllocator::deallocate(void* ptr, size_type n)
    {
        //do nothing for linear allocator
    }
}