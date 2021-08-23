#pragma once
#include "RHI/VulkanRHI.h"
#include "RHI/VulkanRenderShader.h"
#include <unordered_map>

namespace MetaInit
{

	VkPipelineLayoutCreateInfo MakePipelineLayoutCreateInfo(VkPipelineLayoutCreateFlags flags, const Vector<VkDescriptorSetLayout>& ds_layout,
													const Vector<VkPushConstantRange>& push_range);
	VkComputePipelineCreateInfo MakeComputePipelineCreateInfo(VkPipelineCreateFlags flags, VkPipelineLayout layout);
	VkGraphicsPipelineCreateInfo MakeGraphicsPipelineCreateInfo(VkPipelineCreateFlags flags, VkPipelineLayout layout, VkRenderPass render_pass);
	VkPipelineCacheCreateInfo MakePipelineCacheCreateInfo(VkPipelineCacheCreateFlags flags, const Vector<uint8_t>& initial_data);
	VkPipelineShaderStageCreateInfo MakePipelineShaderStageCreateInfo(VkPipelineShaderStageCreateFlags flags, VkShaderStageFlagBits stage);

	class VulkanCmdBuffer;
	class DescriptorSetsWrapper;
	class VulkanRenderPipeline
	{
	public:
		//todo: support task and mesh shader
		enum class EPipeType:uint8_t
		{
			COMPUTE,
			GRAPHICS,
			RAYTRACE,
			COUNT,
		};
		using Ptr = std::unique_ptr<VulkanRenderPipeline>;
		template<typename Parameters, typename ...Args>
		static Ptr Create(VulkanDevice::Ptr device, const Parameters& param, Args... args) {
			throw std::runtime_error("not implement create function"); 
		}
		void Bind(VulkanCmdBuffer& cmd_buffer);
		VkPipeline Get() { return handle_; }
		const VkPipelineLayout GetLayout()const { return layout_; }
		EPipeType Type()const { return pipe_type_; }
		DescriptorSetsWrapper& operator[](std::string& set_name);
		virtual ~VulkanRenderPipeline() { vkDestroyPipeline(device_->Get(), handle_, nullptr); }
	private:
		void BuildDescriptorSets();
	private:
		using DescRepo = std::unordered_map<VkDescriptorSetLayout, DescriptorSetsWrapper>;
		using DescLut = std::unordered_map<std::string, VkDescriptorSetLayout>;
		using ShaderList = Vector<VulkanShaderModule>;
		VulkanDevice::Ptr				device_;
		VkPipeline						handle_{ VK_NULL_HANDLE };
		VkPipelineLayout				layout_{ VK_NULL_HANDLE };
		VkPipelineCache					pipe_cache_{ VK_NULL_HANDLE };
		VkDescriptorUpdateTemplate		desc_template_{ VK_NULL_HANDLE };
		DescRepo						desc_sets_;
		DescLut							desc_lut_;
		ShaderList						shaders_;
		Vector<VkPushConstantRange>		const_ranges_;
		EPipeType						pipe_type_;
	};

	template<>
	VulkanRenderPipeline::Ptr VulkanRenderPipeline::Create(VulkanDevice::Ptr device, const VkGraphicsPipelineCreateInfo& params)
	{
		VulkanRenderPipeline::Ptr pipe(new VulkanRenderPipeline);
		auto ret_val = vkCreateGraphicsPipelines(device->Get(), device->GetPipelineCache(), 1, &params, g_host_alloc, &pipe->handle_);
		assert(ret_val == VK_SUCCESS && "create graphics pipeline failed");
		pipe->pipe_type_ = EPipeType::GRAPHICS;
		pipe->layout_ = params.layout;
		pipe->device_ = device;
		{
			pipe->desc_sets_.clear();
			//for( auto n = 0; n < )
			//pipe->desc_sets_
		}
		return pipe;
	}

	template<>
	VulkanRenderPipeline::Ptr VulkanRenderPipeline::Create(VulkanDevice::Ptr device, const VkComputePipelineCreateInfo& params)
	{
		VulkanRenderPipeline::Ptr pipe(new VulkanRenderPipeline);
		auto ret_val = vkCreateComputePipelines(device->Get(), device->GetPipelineCache(), 1, &params, g_host_alloc, &pipe->handle_);
		pipe->pipe_type_ = EPipeType::COMPUTE;
		pipe->device_ = device;
		return pipe;
	}

	template<>
	VulkanRenderPipeline::Ptr VulkanRenderPipeline::Create(VulkanDevice::Ptr device, const VkRayTracingPipelineCreateInfoKHR& params)
	{
		VulkanRenderPipeline::Ptr pipe(new VulkanRenderPipeline);
		auto ret_val = vkCreateRayTracingPipelinesKHR(device->Get(), nullptr, nullptr, 1, &params, g_host_alloc, &pipe->handle_);
		pipe->pipe_type_ = EPipeType::RAYTRACE;
		pipe->device_ = device;
		return pipe;
	}

	//FIXME not use pipeline cache

}
