#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHIGlobalEntity.h"
#include "RHI/Vulkan/API/VulkanRHI.h"
#include "RHI/Vulkan/RHIResourceAllocatorVulkan.h"

namespace MetaInit::RHI {
	namespace Vulkan
	{
		class RHIGlobalEntityVulkan : public RHIGlobalEntity
		{
		public:
			 void Init(RHIFeatureLevel feature_level) override;
			 void UnInit() override;
			 void CreateGFXPipelineState() override;
			 void CreateComputePipelineState() override;
			 void CreateSampler() override;
			 void CreateViewPoint() override;
			 RHIResource::Ptr CreateConstBuffer(const RHIBufferDesc& desc) override;
			 RHIResource::Ptr CreateStructedBuffer(const RHIBufferDesc& desc) override;
			 RHIResource::Ptr CreateTexture(const RHITextureDesc& desc) override;
			 RHIResource::Ptr CreateUAV(const RHITextureUAVDesc& desc) override;
			 RHIResource::Ptr CreateSRV(const RHITextureSRVDesc& desc) override;
			 //RHIResource::Ptr CreateRayTracingAccelerateStruct() override;
			 RHICommandContext::Ptr CreateCommandBuffer() override;
			//calculate resource video memory size
			 size_t ComputeMemorySize(RHIResource::Ptr res) const override;
			 void SetViewPoint() override;
			 void ResizeViewPoint() override;
			 void Execute(Span<RHICommandContext::Ptr> cmd_buffers) override;
		private:
			void InitInstance();
		private:
			VulkanInstance::SharedPtr	instance_;
			VulkanDevice::SharedPtr	device_;
			//viewport 

			//memory resource manager
			RHIPooledResourceAllocatorVulkan	pooled_repo_;
			RHITransientResourceAllocatorVulkan	transient_repo_;
		};
	}
}
