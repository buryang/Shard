#pragma once
#include "VulkanRHI.h"

namespace MetaInit
{

	VkPipelineLayoutCreateInfo MakePipelineLayoutCreateInfo(const Vector<VkDescriptorSetLayout>& ds_layout,
													const Vector<VkPushConstantRange>& push_range,
													VkPipelineCreateFlags flags);
	VkComputePipelineCreateInfo MakeComputePipelineCreateInfo();
	VkGraphicsPipelineCreateInfo MakeGraphicsPipelineCreateInfo();
	VkPipelineCacheCreateInfo MakePipelineCacheCreateInfo();
	
	class VulkanCmdBuffer;
	class VulkanRenderPipeline
	{
	public:
		//todo: support task and mesh shader
		enum class PipeType:uint8_t
		{
			COMPUTE,
			GRAPHICS,
			RAYTRACE,
			COUNT,
		};
		using Ptr = std::unique_ptr<VulkanRenderPipeline>;
		template<typename Parameters>
		static Ptr Create(const Parameters& param) { 
			throw std::runtime_error("not implement create function"); 
		}
		void Bind(VulkanCmdBuffer& cmd_buffer);
		VkPipeline Get() {
			return handle_;
		}
		~VulkanRenderPipeline() { vkDestroyPipeline(device_.Get(), handle_, nullptr); }
	private:
		VkPipeline			handle_{ VK_NULL_HANDLE };
		VulkanDevice		device_;
		PipeType			pipe_type_;

	};

	template<>
	VulkanRenderPipeline::Ptr VulkanRenderPipeline::Create(const VkGraphicsPipelineCreateInfo& params)
	{
		VulkanRenderPipeline::Ptr pipe(new VulkanRenderPipeline);
		auto ret_val = vkCreateGraphicsPipelines();
		pipe->pipe_type_	= PipeType::GRAPHICS;
		return pipe;
	}

	template<>
	VulkanRenderPipeline::Ptr VulkanRenderPipeline::Create(const VkComputePipelineCreateInfo& params)
	{
		VulkanRenderPipeline::Ptr pipe(new VulkanRenderPipeline);
		auto ret_val = vkCreateComputePipelines();
		pipe->pipe_type_ = PipeType::COMPUTE;
		return pipe;
	}

	template<>
	VulkanRenderPipeline::Ptr VulkanRenderPipeline::Create(const VkRayTracingPipelineCreateInfoKHR& params)
	{
		VulkanRenderPipeline::Ptr pipe(new VulkanRenderPipeline);
		auto ret_val = vkCreateRayTracingPipelinesKHR();
		pipe->pipe_type_ = PipeType::RAYTRACE;
		return pipe;
	}

	//FIXME not use pipeline cache

}
