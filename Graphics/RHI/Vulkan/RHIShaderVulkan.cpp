#include "RHI/Vulkan/RHIShaderVulkan.h"

namespace Shard::RHI::Vulkan {

	static inline VkShaderModuleCreateInfo TransformShaderInitializerToVulkanInfo(const RHIShaderInitializer& initializer) {
		return MakeShaderModuleCreateInfo({ initializer.bytes_, initializer.byte_count_ });
	}

	RHIComputeShaderVulkan::RHIComputeShaderVulkan(const RHIShaderInitializer& initializer):RHIComputeShader(initializer)
	{
		const auto& create_info = TransformShaderInitializerToVulkanInfo(initializer);
		VulkanShaderModule::Init(GetGlobalDevice(), create_info);
	}

	RHIHullShaderVulkan::RHIHullShaderVulkan(const RHIShaderInitializer& initializer):RHIHullShader(initializer)
	{
		const auto& create_info = TransformShaderInitializerToVulkanInfo(initializer);
		VulkanShaderModule::Init(GetGlobalDevice(), create_info);
	}

	RHIDomainShaderVulkan::RHIDomainShaderVulkan(const RHIShaderInitializer& initializer):RHIDomainShader(initializer)
	{
		const auto& create_info = TransformShaderInitializerToVulkanInfo(initializer);
		VulkanShaderModule::Init(GetGlobalDevice(), create_info);
	}

	RHIGeometryShaderVulkan::RHIGeometryShaderVulkan(const RHIShaderInitializer& initializer):RHIGeometryShader(initializer)
	{
		const auto& create_info = TransformShaderInitializerToVulkanInfo(initializer);
		VulkanShaderModule::Init(GetGlobalDevice(), create_info);
	}

	RHIPixelShaderVulkan::RHIPixelShaderVulkan(const RHIShaderInitializer& initializer) :RHIPixelShader(initializer)
	{
		const auto& create_info = TransformShaderInitializerToVulkanInfo(initializer);
		VulkanShaderModule::Init(GetGlobalDevice(), create_info);
	}

	static inline VulkanRenderPipeline::Desc TransformPSOToVulkanDesc(const RHIPipelineStateObjectInitializer& initializer, VkPipelineCache cache) {
		VulkanRenderPipeline::Desc pipe_desc{};
		return pipe_desc;
	}

	//move 
	RHIPipelineStateObjectVulkan::RHIPipelineStateObjectVulkan(const RHIPipelineStateObjectInitializer& initializer):RHIPipelineStateObject(initializer)
	{
		//create  vulkan shader 
		auto& desc = TransformPSOToVulkanDesc(initializer, nullptr);
		//to do; think about convert to vulkanpipeline object and init ?
		if (initializer.type_ == RHIPipelineStateObjectInitializer::EType::eGFX) {
			pso_obj_ = new VulkanGraphicsPipeline;
		}
		else if (initializer.type_ == RHIPipelineStateObjectInitializer::EType::eCompute) {
			pso_obj_ = new VulkanComputePipeline;
		}
		else if (initializer.type_ == RHIPipelineStateObjectInitializer::EType::eRayTrace) {
			pso_obj_ = new VulkanRayTracingPipeline;
		}
		else {
			LOG(ERROR) << "pipeline type not supported now";
		}
		pso_obj_->Init(GetGlobalDevice(), desc);
	}
}