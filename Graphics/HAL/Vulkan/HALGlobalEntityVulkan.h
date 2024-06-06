#pragma once
#include "Utils/CommonUtils.h"
#include "HAL/HALGlobalEntity.h"
#include "HAL/HALTypeTraits.h"
#include "HAL/Vulkan/HALResourcesVulkan.h"
#include "HAL/Vulkan/API/VulkanHAL.h"


namespace Shard::HAL {
    namespace Vulkan
    {
        class HALGlobalEntityVulkan : public HALGlobalEntity
        {
        public:
            using Ptr = HALGlobalEntityVulkan*;
            static Ptr Instance();
            void Init() override;
            void UnInit() override;
            void CreateSampler() override;
            void CreateViewPoint() override;
            HALRenderPass::Handle CreatePass(const HALRenderPassInitializer& pass_initializer) override;
            HALShaderLibraryInterface* GetOrCreateShaderLibrary() override;
            HALPipelineStateObjectLibraryInterface* GetOrCreatePSOLibrary() override;
            HALMemoryResidencyManager* GetOrCreateMemoryResidencyManager() override;
#if defined(DEVELOP_DEBUG_TOOLS)&&defined(ENABLE_IMGUI)
            HALImGuiLayerWrapper* GetImGuiLayerWrapper() override;
#endif
            HALResourceBindlessHeap* GetResourceBindlessHeap() override;
            HALCommandContext* CreateCommandBuffer() override;

            void SetViewPoint() override;
            void ResizeViewPoint() override;

            ~HALGlobalEntityVulkan() {
                UnInit();
            }
        protected:
            HALBuffer* CreateBufferImpl(const HALBufferInitializer& desc)override;
            HALTexture* CreateTextureImpl(const HALTextureInitializer& desc)override;
            void DestroyBufferImpl(HALBuffer* buffer)override;
            void DestroyTextureImpl(HALTexture* texture)override;
        private:
            HALGlobalEntityVulkan();
        private:
            VulkanInstance::SharedPtr   instance_;
            VulkanDevice::SharedPtr  device_;
            //viewport 

            //bindless heap ?
            HALResourceBindlessSetVulkan::SharedPtr    bindless_heap_;

            //imgui layer
#if defined(DEVELIP_DEBUG_TOOLS) && defined(ENABLE_IMGUI)
            HALImGuiLayerWrapperVulkan    imgui_wrapper_;
#endif
        }; 
    }
}
