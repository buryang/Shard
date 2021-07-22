#pragma once
#include "RHI/VulkanRHI.h"
#include <unordered_map>

namespace MetaInit
{

	VkPipelineLayoutCreateInfo MakePipelineLayoutCreateInfo(VkPipelineLayoutCreateFlags flags, const Vector<VkDescriptorSetLayout>& ds_layout,
													const Vector<VkPushConstantRange>& push_range);
	VkComputePipelineCreateInfo MakeComputePipelineCreateInfo(VkPipelineCreateFlags flags, VkPipelineLayout layout);
	VkGraphicsPipelineCreateInfo MakeGraphicsPipelineCreateInfo(VkPipelineCreateFlags flags, VkPipelineLayout layout, VkRenderPass render_pass);
	VkPipelineCacheCreateInfo MakePipelineCacheCreateInfo(VkPipelineCacheCreateFlags flags, const Vector<uint8_t>& initial_data);
	
	class VulkanCmdBuffer;
	class DescriptorSetsWrapper;
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
		template<typename Parameters, typename ...Args>
		static Ptr Create(const Parameters& param, Args... args) {
			throw std::runtime_error("not implement create function"); 
		}
		void Bind(VulkanCmdBuffer& cmd_buffer);
		VkPipeline Get() {
			return handle_;
		}
		DescriptorSetsWrapper& operator[](std::string& set_name);
		~VulkanRenderPipeline() { vkDestroyPipeline(device_->Get(), handle_, nullptr); }
	private:
		void BuildDescriptorSets();
	private:
		using DescRepo = std::unordered_map<VkDescriptorSetLayout, DescriptorSetsWrapper>;
		using DescLut = std::unordered_map<std::string, VkDescriptorSetLayout>;
		VkPipeline						handle_{ VK_NULL_HANDLE };
		VkPipelineLayout				layout_{ VK_NULL_HANDLE };
		DescRepo						desc_sets_;
		DescLut							desc_lut_;
		Vector<VkPushConstantRange>		const_ranges_;
		VulkanDevice::Ptr				device_;
		PipeType						pipe_type_;
	};

	template<>
	VulkanRenderPipeline::Ptr VulkanRenderPipeline::Create(const VkGraphicsPipelineCreateInfo& params)
	{
		VulkanRenderPipeline::Ptr pipe(new VulkanRenderPipeline);
		auto ret_val = vkCreateGraphicsPipelines(nullptr, nullptr, 1, &params, g_host_alloc, &pipe->handle_);
		assert(ret_val == VK_SUCCESS && "create graphics pipeline failed");
		pipe->pipe_type_ = PipeType::GRAPHICS;
		pipe->layout_ = params.layout;
		{
			pipe->desc_sets_.clear();
			//for( auto n = 0; n < )
			//pipe->desc_sets_
		}
		return pipe;
	}

	template<>
	VulkanRenderPipeline::Ptr VulkanRenderPipeline::Create(const VkComputePipelineCreateInfo& params)
	{
		VulkanRenderPipeline::Ptr pipe(new VulkanRenderPipeline);
		auto ret_val = vkCreateComputePipelines(nullptr, nullptr, 1, &params, g_host_alloc, &pipe->handle_);
		pipe->pipe_type_ = PipeType::COMPUTE;
		return pipe;
	}

	template<>
	VulkanRenderPipeline::Ptr VulkanRenderPipeline::Create(const VkRayTracingPipelineCreateInfoKHR& params)
	{
		VulkanRenderPipeline::Ptr pipe(new VulkanRenderPipeline);
		auto ret_val = vkCreateRayTracingPipelinesKHR(nullptr, nullptr, nullptr, 1, &params, g_host_alloc, &pipe->handle_);
		pipe->pipe_type_ = PipeType::RAYTRACE;
		return pipe;
	}

	//FIXME not use pipeline cache

}
