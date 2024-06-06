#include "HAL/HALShaderLibrary.h"

namespace Shard::HAL {
    
    HALBlendStateInitializer::HashType HALBlendStateInitializer::ComputeHash(const HALBlendStateInitializer& initializer)
    {
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);

        HALBlendStateInitializer::HashType hash;
        blake3_hasher_finalize(&hasher, hash.GetBytes(), hash.GetHashSize());
        return hash;
    }

    HALDepthStencilStateInitializer::HashType HALDepthStencilStateInitializer::ComputeHash(const HALDepthStencilStateInitializer& initializer)
    {
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);

        HALDepthStencilStateInitializer::HashType hash;
        blake3_hasher_finalize(&hasher, hash.GetBytes(), hash.GetHashSize());
        return hash;
    }

    HALRasterizationStateInitializer::HashType HALRasterizationStateInitializer::ComputeHash(const HALRasterizationStateInitializer& initializer)
    {
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);

        HALRasterizationStateInitializer::HashType hash;
        blake3_hasher_finalize(&hasher, hash.GetBytes(), hash.GetHashSize());
        return hash;
    }

    HALVertexInputStateInitializer::HashType HALVertexInputStateInitializer::ComputeHash(const HALVertexInputStateInitializer& initializer)
    {
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);

        HALVertexInputStateInitializer::HashType hash;
        return hash;
    }
    
    void HALPipelineStateObjectLibraryInterface::UnInit(void)
    {
        for (auto [_, ptr] : pso_cache_) {
            DestoryPipelineImpl(ptr);
            //delete ptr;
        }
    }

    HALPipelineStateObject* HALPipelineStateObjectLibraryInterface::GetOrCreatEPipeline(const HALPipelineStateObjectInitializer& initializer)
    {
        {
            std::shared_lock<std::shared_mutex> lock(cache_mutex_);
            if (auto iter = pso_cache_.find(initializer); iter != pso_cache_.end()) {
                return iter->second;
            }
        }

        //to do dual-lock logic
        {
            std::unique_lock<std::shared_mutex> lock(cache_mutex_);
            if (auto iter = pso_cache_.find(initializer); iter != pso_cache_.end()) {
                return iter->second;
            }
            auto pso_ptr = CreatEPipelineImpl(initializer);
            PCHECK(pso_ptr != nullptr) << "create pso rhi failed";
            if (pso_ptr) {
                pso_cache_.insert({initializer, pso_ptr });
            }
            return pso_ptr;
        }
    }
    
    constexpr size_type HALPipelineStateObjectLibraryInterface::GetPSOCount() const
    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        return pso_cache_.size();
    }

    HALPipelineStateObjectLibraryInterface::~HALPipelineStateObjectLibraryInterface()
    {
        UnInit();
    }

    /*-----rhi shader define -------*/
    HALShader::~HALShader() {}
    HALPipelineStateObject::~HALPipelineStateObject() {}
}