#pragma once
#include "RHI/RHIResources.h"

namespace Shard::RHI
{
    class RHIPooledResourceAllocator final
    {
    public:
        using Ptr = RHIPooledResourceAllocator*;
        RHIPooledResourceAllocator();
        RHIBuffer::Ptr FindBuffer(const RHIBufferInitializer& desc);
        RHITexture::Ptr FindTexture(const RHITextureInitializer& desc);
        bool RegistBuffer(RHIBuffer::Ptr buffer, uint32_t frame_index);
        bool RegistTexture(RHITexture::Ptr texture, uint32_t frame_index);
        void Tick(uint32_t frame_index);
        void Clear();
        virtual ~RHIPooledResourceAllocator() { Clear(); };
    protected:
        uint32_t ComputeBufferInitializerHash(const RHIBufferInitializer& desc)const;
        uint32_t ComputeTextureInitializerHash(const RHITextureInitializer& desc)const;
        bool IsBufferCompatiable(const RHIBuffer::Ptr buffer, const RHIBufferInitializer& desc)const;
        bool IsTextureCompatiable(const RHITexture::Ptr texture, const RHITextureInitializer& desc)const;
    protected:
        struct PoolResource
        {
            uint32_t    hash_{ 0u };
            uint32_t    last_time_stamp_{ 0u };
            RHIResource::Ptr    resource_{ nullptr };
        };
        using PoolResourceAllocator = std::remove_pointer_t<decltype(g_rhi_allocator)>::rebind<PoolResource>::other;
        Vector<PoolResource, PoolResourceAllocator>    pooled_resources_;
        RHISizeType total_size_{ 0u };
    };
}
