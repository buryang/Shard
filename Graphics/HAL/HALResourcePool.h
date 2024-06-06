#pragma once
#include "HAL/HALResources.h"

namespace Shard::HAL
{
    class HALPooledResourceAllocator final
    {
    public:
        HALPooledResourceAllocator();
        HALBuffer* FindBuffer(const HALBufferInitializer& desc);
        HALTexture* FindTexture(const HALTextureInitializer& desc);
        bool RegistBuffer(HALBuffer* buffer, uint32_t frame_index);
        bool RegistTexture(HALTexture* texture, uint32_t frame_index);
        void Tick(uint32_t frame_index);
        void Clear();
        virtual ~HALPooledResourceAllocator() { Clear(); };
    protected:
        uint32_t ComputeBufferInitializerHash(const HALBufferInitializer& desc)const;
        uint32_t ComputeTextureInitializerHash(const HALTextureInitializer& desc)const;
        bool IsBufferCompatiable(const HALBuffer* buffer, const HALBufferInitializer& desc)const;
        bool IsTextureCompatiable(const HALTexture* texture, const HALTextureInitializer& desc)const;
    protected:
        struct PoolResource
        {
            uint32_t    hash_{ 0u };
            uint32_t    last_time_stamp_{ 0u };
            HALResource*    resource_{ nullptr };
        };
        using PoolResourceAllocator = std::remove_pointer_t<decltype(g_hal_allocator)>::rebind<PoolResource>::other;
        Vector<PoolResource, PoolResourceAllocator>    pooled_resources_;
        HALSizeType total_size_{ 0u };
    };
}
