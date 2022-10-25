#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHIGlobalEntity.h"
#include "RHI/Vulkan/API/VulkanRHI.h"

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
			 RHIResource::Ptr CreateConstBuffer() override;
			 RHIResource::Ptr CreateStructedBuffer() override;
			 RHIResource::Ptr CreateTexture() override;
			 RHIResource::Ptr CreateUAV() override;
			 RHIResource::Ptr CreateSRV() override;
			 RHIResource::Ptr CreateRayTracingAccelerateStruct() override;
			 RHICommandContext::Ptr CreateCommandBuffer() override;
			//calculate resource video memory size
			 size_t ComputeMemorySize(RHIResource::Ptr res) const override;
			 void SetViewPoint() override;
			 void ResizeViewPoint() override;
			 void Execute(Span<RHICommandContext::Ptr> cmd_buffers) override;
		private:
			VulkanInstance::SharedPtr	instance_;
		};
	}
}
