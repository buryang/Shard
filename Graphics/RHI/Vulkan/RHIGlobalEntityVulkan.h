#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHIGlobalEntity.h"
#include "RHI/RHITypeTraits.h"
#include "RHI/Vulkan/RHIResourcesVulkan.h"
#include "RHI/Vulkan/API/VulkanRHI.h"


namespace Shard::RHI {
    namespace Vulkan
    {
        class RHIGlobalEntityVulkan : public RHIGlobalEntity
        {
        public:
            using Ptr = RHIGlobalEntityVulkan*;
            static Ptr Instance();
            void Init() override;
            void UnInit() override;
            void CreateSampler() override;
            void CreateViewPoint() override;
            RHIShaderLibraryInterface::Ptr GetOrCreateShaderLibrary() override;
            RHIPipelineStateObjectLibraryInterface::Ptr GetOrCreatePSOLibrary() override;
#if defined(DEVELOP_DEBUG_TOOLS)&&defined(ENABLE_IMGUI)
            RHIImGuiLayerWrapper::Ptr GetImGuiLayerWrapper() override;
#endif
            RHIResourceBindlessHeap::SharedPtr GetResourceBindlessHeap() override;
            RHIBuffer::Ptr CreateUniformBuffer(const RHIBufferInitializer& desc) override;
            RHIBuffer::Ptr CreateStructedBuffer(const RHIBufferInitializer& desc) override;
            RHITexture::Ptr CreateTexture(const RHITextureInitializer& desc) override;
            RHIResource::Ptr CreateUAV(const RHITextureUAVInitializer& desc) override;
            RHIResource::Ptr CreateSRV(const RHITextureSRVInitializer& desc) override;
            //RHIResource::Ptr CreateRayTracingAccelerateStruct() override;
            RHICommandContext::Ptr CreateCommandBuffer() override;
            //calculate resource video memory size
            size_t ComputeMemorySize(RHIResource::Ptr res) const override;
            void SetViewPoint() override;
            void ResizeViewPoint() override;
            void Execute(Span<RHICommandContext::Ptr> cmd_buffers) override;
            bool SetUpTexture(RHITextureVulkan::Ptr texture);
            bool SetUpBuffer(RHIBufferVulkan::Ptr buffer);
            //special functions for vulkan
            FORCE_INLINE VulkanInstance::SharedPtr GetVulkanInstance() {
                assert(nullptr != instance_);
                return instance_;
            }
            FORCE_INLINE VulkanDevice::SharedPtr GetVulkanDevice() {
                assert(nullptr != device_);
                return device_;
            }
            ~RHIGlobalEntityVulkan() {
                UnInit();
            }
        protected:

        private:
            RHIGlobalEntityVulkan();
            void InitInstance();
            void InitGlobalAllocationCallBacks();
        private:
            VulkanInstance::SharedPtr    instance_;
            VulkanDevice::SharedPtr    device_;
            //viewport 

            //memory resource manager
            RHIPooledTextureAllocatorVulkan    pooled_repo_;
            RHITransientResourceAllocatorVulkan::SharedPtr transient_repo_;

            //bindless heap ?
            RHIResourceBindlessSetVulkan::SharedPtr    bindless_heap_;

            //imgui layer
#if defined(DEVELIP_DEBUG_TOOLS) && defined(ENABLE_IMGUI)
            RHIImGuiLayerWrapperVulkan    imgui_wrapper_;
#endif
        }; 
    }
}
