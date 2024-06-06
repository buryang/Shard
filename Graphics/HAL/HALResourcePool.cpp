#include "Utils/Hash.h"
#include "HALResourcePool.h"

namespace Shard::HAL
{
    HALPooledResourceAllocator::HALPooledResourceAllocator():pooled_resources_(*g_hal_allocator)
    {
    }
    HALBuffer* HALPooledResourceAllocator::FindBuffer(const HALBufferInitializer& desc)
    {
        const auto hash = ComputeBufferInitializerHash(desc);
        auto begin_iter = pooled_resources_.cbegin();
        for (;;) {
            auto iter = eastl::find_if(begin_iter, pooled_resources_.cend(), [&](const auto val) { return val.hash_ == hash; });
            if (iter == pooled_resources_.cend()) {
                break;
            }
            if (/*check is a buffer*/&&IsBufferCompatiable(iter->resource_, desc)) {
                return iter->resource_;
            }
            begin_iter = iter;
        }
        return nullptr;
    }
    HALTexture* HALPooledResourceAllocator::FindTexture(const HALTextureInitializer& desc)
    {
        const auto hash = ComputeTextureInitializerHash(desc);
        auto begin_iter = pooled_resources_.cbegin();
        for (;;) {
            auto iter = eastl::find_if(begin_iter, pooled_resources_.cend(), [&](const auto val) {return val.hash_ == hash; });
            if (iter == pooled_resources_.cend()) {
                break;
            }
            if (/*check it's a texture*/ && IsTextureCompatiable(iter->resource_, desc)) {
                return iter->resource_;
            }
            begin_iter = iter;
        }
        return nullptr;
    }
    bool HALPooledResourceAllocator::RegistBuffer(HALBuffer* buffer, uint32_t frame_index)
    {
        const auto buffer_desc = buffer->GetBufferDesc();
        if (total_size_ + buffer_desc.size_ < 0u) { 
            const auto hash = ComputeBufferInitializerHash(*buffer);
            pooled_resources_.emplace_back(PoolResource{ hash, frame_index, buffer });
            return true;
        }
        return false;
    }
    bool HALPooledResourceAllocator::RegistTexture(HALTexture* texture, uint32_t frame_index)
    {
        const auto texture_desc = texture->GetTextureDesc();
        if (total_size_ + texture_desc.siz) {
            const auto hash = ComputeTextureInitializerHash(texture_desc);
            pooled_resources_.emplace_back(PoolResource{ hash, frame_index, texture });
            return true;
        }
        return false;
    }
    void HALPooledResourceAllocator::Tick(uint32_t frame_index)
    {
        auto max_frame_gap = GET_PARAM_TYPE_VAL(UINT, HAL_RESOURCE_POOL_FRAME_GAP);
        auto max_resouce_size = GET_PARAM_TYPE_VAL(UINT64, HAL_RESOURCE_POOL_MAX_SIZE);
        bool force_release{ true };
        do {
            for (auto iter = pooled_resources_.begin(); iter != pooled_resources_.end(); ++iter) {
                if (frame_index - iter->last_time_stamp_ >= max_frame_gap) {
                    auto* resource = iter->resource_;
                    total_size_ -= iter->resource_->GetOccupySize();
                    delete resource; //todo fixit 
                    pooled_resources_.erase(iter);
                }
                if (total_size_ < max_resouce_size || !force_release) {
                    break;
                }
                force_release = --max_frame_gap > 1u;

            }
        }while (1);
        eastl::sort(pooled_resources_.begin(), pooled_resources_.end(), [](auto lhs, auto rhs) { return lhs.hash_ > rhs.hash_; });
    }

    void HALPooledResourceAllocator::Clear()
    {
        for (auto iter = pooled_resources_.begin(); iter != pooled_resources_.end(); ++iter) {
            delete iter->resource_;//todo
        }
        pooled_resources_.clear();
    }

    bool HALPooledResourceAllocator::IsBufferCompatiable(const HALBuffer* buffer, const HALBufferInitializer& desc) const
    {
        const auto& buffer_desc = buffer->GetBufferDesc();
        return buffer_desc.access_ == desc.access_ && buffer_desc.type_ == desc.type_
            && buffer_desc.is_dedicated_==desc.is_dedicated_ && buffer_desc.is_transiant_ == desc.is_transiant_
            && buffer_desc.size_ >= desc.size_;
    }

    bool HALPooledResourceAllocator::IsTextureCompatiable(const HALTexture* texture, const HALTextureInitializer& desc) const
    {
        const auto& texture_desc = texture->GetTextureDesc();
        return texture_desc.type_ == desc.type_ && texture_desc.format_ == desc.format_
            && texture_desc.access_ == desc.access_; //todo
    }

    uint32_t HALPooledResourceAllocator::ComputeBufferInitializerHash(const HALBufferInitializer& desc)const
    {
        uint64_t hash0, hash1;
        folly::hash::SpookyHashV2 hasher;
        hasher.Init(typeid(HALBufferInitializer).hash_code(), (uint64_t)this);
        hasher.Update(&(desc.type_), sizeof(desc.type_));
        hasher.Update(&(desc.access_), sizeof(desc.access_));
        //todo
        hasher.Final(&hash0, &hash1);
        return *((uint32_t*)&hash0);
    }
    uint32_t HALPooledResourceAllocator::ComputeTextureInitializerHash(const HALTextureInitializer& desc)const
    {
        uint64_t hash0, hash1;
        folly::hash::SpookyHashV2 hasher;
        hasher.Init(typeid(HALTextureInitializer).hash_code(), (uint64_t)this);
        hasher.Update(&desc.type_, sizeof(desc.type_));
        hasher.Update(&desc.layout_, sizeof(desc.layout_));
        hasher.Update(&desc.access_, sizeof(desc.access_));
        //todo dedicate / transient  and etc.
        hasher.Final(&hash0, &hash1);
        return *((uint32_t*)&hash0);
    }
}
