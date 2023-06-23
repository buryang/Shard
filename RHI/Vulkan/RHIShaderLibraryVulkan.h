#include "RHI/RHIShaderLibrary.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"
#include <mutex>

namespace Shard::RHI::Vulkan {

	using RHIShaderLibraryVulkan = RHIPseudoShaderLibrary<RHIGlobalEntityVulkan>;

	template<typename Desc, typename Entity, typename Creator>
	class RHIImmutablePipelineStateObjectFactoryVulkan
	{
	public:
		using Key = Desc::HashType;
		using Type = Entity;
		static RHIImmutablePipelineStateObjectFactoryVulkan& Instance() {
			static RHIImmutablePipelineStateObjectFactoryVulkan instance;
			return instance;
		}
		const Type* GetOrCreateEntity(const Desc& desc) {
			const auto& key = Desc::ComputeHash(desc);
			if (auto iter = immutable_entities_.find(key); iter != immutable_entities_.end()) {
				return iter->second;
			}
			auto entity = Creator.Create(desc); //to do
			immutable_entities_.insert({ key, entity });
			return entity;
		}
		void Release() {
			for (auto& [_, entity] : immutable_entities) {
				Creator::Destroy(entity);
			}
		}
		~RHIImmutablePipelineStateObjectFactoryVulkan() { Release(); }
	private:
		DISALLOW_COPY_AND_ASSIGN(RHIImmutablePipelineStateObjectFactoryVulkan);
	private:
		Map<Key, Type*>	immutable_entities_;
	};

	class BlendStateVulkanCreator {
	public:
		static VkPipelineColorBlendStateCreateInfo* Create(const RHIBlendStateInitializer& initializer);
		static void Destroy(VkPipelineColorBlendStateCreateInfo*);
	};
	class RasterizationStateVulkanCreator {
	public:
		static VkPipelineRasterizationStateCreateInfo* Create(const RHIRasterizationStateInitializer& initializer);
		static void Destroy(VkPipelineRasterizationStateCreateInfo*);
	};
	class DepthStencilStateVulkanCreator {
	public:
		static VkPipelineDepthStencilStateCreateInfo* Create(const RHIDepthStencilStateInitializer& initializer);
		static void Destroy(VkPipelineDepthStencilStateCreateInfo*);
	};

	using RHIImmutableBlendStateFactoryVulkan = RHIImmutablePipelineStateObjectFactoryVulkan<RHIBlendStateInitializer, VkPipelineColorBlendStateCreateInfo, BlendStateVulkanCreator>;
	using RHIImmutableRasterizationStateFactoryVulkan = RHIImmutablePipelineStateObjectFactoryVulkan<RHIRasterizationStateInitializer, VkPipelineRasterizationStateCreateInfo, RasterizationStateVulkanCreator>;
	using RHIImmutableDepthStencilStateFactoryVulkan = RHIImmutablePipelineStateObjectFactoryVulkan<RHIDepthStencilStateInitializer, VkPipelineDepthStencilStateCreateInfo, DepthStencilStateVulkanCreator>;

	class PipelineStateObjectCacheVulkan;
	class RHIPipelineStateObjectLibraryVulkan : public RHIPipelineStateObjectLibraryInterface
	{
	public:
		void Init() override;
		~RHIPipelineStateObjectLibraryVulkan();
	protected:
		RHIPipelineStateObject::Ptr CreatePipelineImpl(const RHIPipelineStateObjectInitializer& initializer) override;
	private:
		SmallVector<PipelineStateObjectCacheVulkan*>	pso_caches_;
	};
}