#include "HAL/HALShaderLibrary.h"
#include "HAL/Vulkan/HALGlobalEntityVulkan.h"
#include <mutex>

namespace Shard::HAL::Vulkan {

    using HALShaderLibraryVulkan = HALPseudoShaderLibrary<HALGlobalEntityVulkan>;

    template<typename Desc, typename Entity, typename Creator>
    class HALImmutablEPipelineStateObjectFactoryVulkan
    {
    public:
        using Key = Desc::HashType;
        using Type = Entity;
        static HALImmutablEPipelineStateObjectFactoryVulkan& Instance() {
            static HALImmutablEPipelineStateObjectFactoryVulkan instance;
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
        ~HALImmutablEPipelineStateObjectFactoryVulkan() { Release(); }
    private:
        DISALLOW_COPY_AND_ASSIGN(HALImmutablEPipelineStateObjectFactoryVulkan);
    private:
        Map<Key, Type*>    immutable_entities_;
    };

    class BlendStateVulkanCreator {
    public:
        static VkPipelineColorBlendStateCreateInfo* Create(const HALBlendStateInitializer& initializer);
        static void Destroy(VkPipelineColorBlendStateCreateInfo*);
    };
    class RasterizationStateVulkanCreator {
    public:
        static VkPipelineRasterizationStateCreateInfo* Create(const HALRasterizationStateInitializer& initializer);
        static void Destroy(VkPipelineRasterizationStateCreateInfo*);
    };
    class DepthStencilStateVulkanCreator {
    public:
        static VkPipelineDepthStencilStateCreateInfo* Create(const HALDepthStencilStateInitializer& initializer);
        static void Destroy(VkPipelineDepthStencilStateCreateInfo*);
    };

    using HALImmutableBlendStateFactoryVulkan = HALImmutablEPipelineStateObjectFactoryVulkan<HALBlendStateInitializer, VkPipelineColorBlendStateCreateInfo, BlendStateVulkanCreator>;
    using HALImmutableRasterizationStateFactoryVulkan = HALImmutablEPipelineStateObjectFactoryVulkan<HALRasterizationStateInitializer, VkPipelineRasterizationStateCreateInfo, RasterizationStateVulkanCreator>;
    using HALImmutableDepthStencilStateFactoryVulkan = HALImmutablEPipelineStateObjectFactoryVulkan<HALDepthStencilStateInitializer, VkPipelineDepthStencilStateCreateInfo, DepthStencilStateVulkanCreator>;

    class PipelineStateObjectCacheVulkan;
    class HALPipelineStateObjectLibraryVulkan : public HALPipelineStateObjectLibraryInterface
    {
    public:
        void Init() override;
        ~HALPipelineStateObjectLibraryVulkan();
    protected:
        HALPipelineStateObject* CreatEPipelineImpl(const HALPipelineStateObjectInitializer& initializer) override;
    private:
        SmallVector<PipelineStateObjectCacheVulkan*>    pso_caches_;
    };
}