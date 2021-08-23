#include "RHI/VulkanRenderPipeline.h"

#include "Utils/CommonUtils.h"
#include "RHI/VulkanCmdContext.h"


namespace MetaInit
{
	VkPipelineLayoutCreateInfo MakePipelineLayoutCreateInfo(VkPipelineLayoutCreateFlags flags, const Vector<VkDescriptorSetLayout>& ds_layout,
															const Vector<VkPushConstantRange>& push_range)
	{
		VkPipelineLayoutCreateInfo layout_info;
		memset(&layout_info, sizeof(VkPipelineLayoutCreateInfo), 1);
		layout_info.flags = flags;
		if (push_range.empty())
		{
			layout_info.pushConstantRangeCount = 0;
		}
		else
		{
			layout_info.pushConstantRangeCount = push_range.size();
			layout_info.pPushConstantRanges = push_range.data();
		}

		if (ds_layout.empty())
		{
			layout_info.setLayoutCount = 0;
		}
		else
		{
			layout_info.setLayoutCount = ds_layout.size();
			layout_info.pSetLayouts = ds_layout.data();
		}

		return layout_info;
	}

	VkComputePipelineCreateInfo MakeComputePipelineCreateInfo(VkPipelineCreateFlags flags, VkPipelineLayout layout)
	{
		VkComputePipelineCreateInfo pipe_info;
		memset(&pipe_info, sizeof(VkComputePipelineCreateInfo), 1);
		pipe_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipe_info.flags = flags;
		pipe_info.layout = layout;
		//todo
		return pipe_info;
	}

	VkGraphicsPipelineCreateInfo MakeGraphicsPipelineCreateInfo(VkPipelineCreateFlags flags, VkPipelineLayout layout, VkRenderPass render_pass)
	{
		VkGraphicsPipelineCreateInfo pipe_info;
		memset(&pipe_info, sizeof(VkGraphicsPipelineCreateInfo), 1);
		pipe_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipe_info.flags = flags;
		pipe_info.renderPass = render_pass;
		//todo
		return pipe_info;
	}

	VkPipelineCacheCreateInfo MakePipelineCacheCreateInfo(VkPipelineCacheCreateFlags flags, const Vector<uint8_t>& initial_data)
	{
		VkPipelineCacheCreateInfo cache_info;
		memset(&cache_info, sizeof(VkPipelineCacheCreateInfo), 1);
		cache_info.flags = flags;
		cache_info.initialDataSize = initial_data.size();
		cache_info.pInitialData = initial_data.data();
		return cache_info;
	}

	static inline VkPipelineInputAssemblyStateCreateInfo MakeVkPipelineInputAssemblyStateCreateInfo()
	{
		VkPipelineInputAssemblyStateCreateInfo assemble_info{};
		memset(&assemble_info, 0, sizeof(assemble_info));
		assemble_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		return assemble_info;
	}

	static inline VkPipelineRasterizationStateCreateInfo MakeVkPipelineRasterizationStateCreateInfo()
	{
		VkPipelineRasterizationStateCreateInfo raster_info{};
		memset(&raster_info, 0, sizeof(raster_info));
		raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		return raster_info;
	}

	static inline VkPipelineColorBlendStateCreateInfo MakeVkPipelineColorBlendStateCreateInfo()
	{
		VkPipelineColorBlendStateCreateInfo color_info{};
		memset(&color_info, 0, sizeof(color_info));
		color_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		return color_info;
	}

	namespace
	{
		inline VkPipelineBindPoint TransPipeTypeToBindPoint(VulkanRenderPipeline::EPipeType type)
		{
			using SrcType = VulkanRenderPipeline::EPipeType;
			switch (type)
			{
			case SrcType::COMPUTE:
				return VK_PIPELINE_BIND_POINT_COMPUTE;
			case SrcType::GRAPHICS:
				return VK_PIPELINE_BIND_POINT_GRAPHICS;
			case SrcType::RAYTRACE:
				return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
			default:
				throw std::invalid_argument("no such bind point");
			}
		}
	}

	void VulkanRenderPipeline::Bind(VulkanCmdBuffer& cmd_buffer)
	{
		auto bind_point = TransPipeTypeToBindPoint(pipe_type_);
		vkCmdBindPipeline(cmd_buffer.Get(), bind_point, handle_);

		//todo when to bind descs, already do update work
		SmallVector<VkDescriptorSet> bind_sets;
		for (auto& desc_pair : desc_sets_)
		{
			bind_sets.push_back(desc_pair.second.Get());
		}
		vkCmdBindDescriptorSets(cmd_buffer.Get(), bind_point, layout_, 0, bind_sets.size(), bind_sets.data(), 0, nullptr);
	}

	DescriptorSetsWrapper& VulkanRenderPipeline::operator[](std::string& set_name)
	{
		return desc_sets_[desc_lut_[set_name]];
	}

}