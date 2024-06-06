#include "HAL/HALGlobalEntity.h"

namespace Shard::HAL
{
    HALGlobalEntity* HALGlobalEntity::Instance()
    {
        auto back_end = static_cast<EHALBackEnd>(GET_PARAM_TYPE_VAL(UINT, HAL_ENTITY_BACKEND));
        auto iter = create_func_repo_.find(back_end);
        if (!IsBackEndSupported(back_end) || iter == create_func_repo_.end()) {
            return nullptr;
        }
        return (iter->second)();
    }

    bool HALGlobalEntity::RegistCreateFunc(EHALBackEnd back_end, CreateFunc&& func)
    {
        if (!IsBackEndSupported(back_end) || create_func_repo_.find(back_end) != create_func_repo_.end()) {
            PLOG(ERROR) << "regist backend(" << static_cast<uint32_t>(back_end) << "create function failed" << std::endl;
        }
        create_func_repo_.insert(eastl::make_pair(back_end, func));
        return true;
    }
    
    HALShaderLibraryInterface* HALGlobalEntity::GetOrCreateShaderLibrary()
    {
        LOG(ERROR) << "current backend not support shader library";
        return nullptr;
    }

    HALBuffer* HALGlobalEntity::CreateBuffer(const HALBufferInitializer& desc)
    {
        return CreateBufferImpl(desc);
    }

    HALTexture* HALGlobalEntity::CreateTexture(const HALTextureInitializer& desc)
    {
        if (GET_PARAM_TYPE_VAL(BOOL, HAL_RESOURCE_POOL_ENABLE)) {
            auto* pooled_texture = pooled_allocator_.FindTexture(desc);
            if (pooled_texture) {
                return pooled_texture;
            }
        }
        return CreateTextureImpl(desc);

    }

    void HALGlobalEntity::ReleaseTexture(HALTexture* texture)
    {
        if (GET_PARAM_TYPE_VAL(BOOL, HAL_RESOURCE_POOL_ENABLE)) {
            auto result = pooled_allocator_.RegistTexture(texture);
            if (result) {
                return;
            }
        }
        DestroyTextureImpl(texture);
    }

    void HALGlobalEntity::ReleaseBuffer(HALBuffer* buffer)
    {
        DestroyBufferImpl(buffer);
    }

    Utils::ScalablePoolAllocator<uint8_t>* g_hal_allocator = Utils::TLSScalablePoolAllocatorInstance<uint8_t,POOL_HAL_ID>();
}