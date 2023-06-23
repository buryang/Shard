#include "RHI/RHIShaderLibrary.h"

namespace Shard::RHI {
	
	RHIBlendStateInitializer::HashType RHIBlendStateInitializer::ComputeHash(const RHIBlendStateInitializer& initializer)
	{
		blake3_hasher hasher;
		blake3_hasher_init(&hasher);

		RHIBlendStateInitializer::HashType hash;
		blake3_hasher_finalize(&hasher, hash.GetBytes(), hash.GetHashSize());
		return hash;
	}

	RHIDepthStencilStateInitializer::HashType RHIDepthStencilStateInitializer::ComputeHash(const RHIDepthStencilStateInitializer& initializer)
	{
		blake3_hasher hasher;
		blake3_hasher_init(&hasher);

		RHIDepthStencilStateInitializer::HashType hash;
		blake3_hasher_finalize(&hasher, hash.GetBytes(), hash.GetHashSize());
		return hash;
	}

	RHIRasterizationStateInitializer::HashType RHIRasterizationStateInitializer::ComputeHash(const RHIRasterizationStateInitializer& initializer)
	{
		blake3_hasher hasher;
		blake3_hasher_init(&hasher);

		RHIRasterizationStateInitializer::HashType hash;
		blake3_hasher_finalize(&hasher, hash.GetBytes(), hash.GetHashSize());
		return hash;
	}

	RHIVertexInputStateInitializer::HashType RHIVertexInputStateInitializer::ComputeHash(const RHIVertexInputStateInitializer& initializer)
	{
		blake3_hasher hasher;
		blake3_hasher_init(&hasher);

		RHIVertexInputStateInitializer::HashType hash;
		return hash;
	}
	
	RHIPipelineStateObject::Ptr RHIPipelineStateObjectLibraryInterface::GetOrCreatePipeline(const RHIPipelineStateObjectInitializer& initializer)
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
			auto pso_ptr = CreatePipelineImpl(initializer);
			PCHECK(pso_ptr != nullptr) << "create pso rhi failed";
			if (pso_ptr) {
				pso_cache_.insert({initializer, pso_ptr });
			}
			return pso_ptr;
		}
	}
	
	constexpr size_type RHIPipelineStateObjectLibraryInterface::GetPSOCount() const
	{
		std::shared_lock<std::shared_mutex> lock(cache_mutex_);
		return pso_cache_.size();
	}

	RHIPipelineStateObjectLibraryInterface::~RHIPipelineStateObjectLibraryInterface()
	{
		for (auto [_, ptr] : pso_cache_) {
			delete ptr;
		}
	}

	/*-----rhi shader define -------*/
	RHIShader::~RHIShader() {}
	RHIPipelineStateObject::~RHIPipelineStateObject() {}
}