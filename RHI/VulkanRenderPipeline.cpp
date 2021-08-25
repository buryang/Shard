#include "RHI/VulkanRenderPipeline.h"

#include "Utils/CommonUtils.h"
#include "RHI/VulkanCmdContext.h"


namespace MetaInit
{

	VkPipelineCacheCreateInfo MakePipelineCacheCreateInfo(VkPipelineCacheCreateFlags flags, const Vector<uint8_t>& initial_data)
	{
		VkPipelineCacheCreateInfo cache_info;
		memset(&cache_info, sizeof(VkPipelineCacheCreateInfo), 1);
		cache_info.flags = flags;
		cache_info.initialDataSize = initial_data.size();
		cache_info.pInitialData = initial_data.data();
		return cache_info;
	}

	static inline VkPipelineLayoutCreateInfo MakePipelineLayoutCreateInfo(const Vector<VkDescriptorSetLayout>& ds_layout, const Vector<VkPushConstantRange>& push_range)
	{
		VkPipelineLayoutCreateInfo layout_info;
		memset(&layout_info, sizeof(VkPipelineLayoutCreateInfo), 1);
		if (!push_range.empty())
		{
			layout_info.pushConstantRangeCount = push_range.size();
			layout_info.pPushConstantRanges = push_range.data();
		}

		if (!ds_layout.empty())
		{
			layout_info.setLayoutCount = ds_layout.size();
			layout_info.pSetLayouts = ds_layout.data();
		}

		return layout_info;
	}

	static inline VkPipelineVertexInputStateCreateInfo MakeVkPipelineVertexInputStateCreateInfo(const Vector<VkVertexInputBindingDescription>& input_descs,
																								const Vector<VkVertexInputAttributeDescription>& attribute_descs)
	{
		VkPipelineVertexInputStateCreateInfo vertex_info{};
		memset(&vertex_info, 0, sizeof(vertex_info));
		vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		if (!input_descs.empty())
		{
			vertex_info.vertexBindingDescriptionCount = input_descs.size();
			vertex_info.pVertexBindingDescriptions = input_descs.data();
		}
		if (!attribute_descs.empty())
		{
			vertex_info.vertexAttributeDescriptionCount = attribute_descs.size();
			vertex_info.pVertexAttributeDescriptions = attribute_descs.data();
		}
		return vertex_info;
	}

	static inline VkPipelineInputAssemblyStateCreateInfo MakeVkPipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology)
	{
		VkPipelineInputAssemblyStateCreateInfo assemble_info{};
		memset(&assemble_info, 0, sizeof(assemble_info));
		assemble_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		assemble_info.topology = topology;
		assemble_info.primitiveRestartEnable = VK_FALSE; //todo 
		return assemble_info;
	}

	static inline VkPipelineViewportStateCreateInfo MakeVkPipelineViewportStateCreateInfo(const Vector<VkViewport>& viewpoints, const Vector<VkRect2D>& scissors)
	{
		VkPipelineViewportStateCreateInfo viewpoint_info{};
		memset(&viewpoint_info, 0, sizeof(viewpoint_info));
		viewpoint_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		if (!viewpoints.empty())
		{
			viewpoint_info.viewportCount = viewpoints.size();
			viewpoint_info.pViewports = viewpoints.data();
		}
		if (!scissors.empty())
		{
			viewpoint_info.scissorCount = scissors.size();
			viewpoint_info.pScissors = scissors.data();
		}
		return viewpoint_info;
	}

	static inline VkPipelineRasterizationStateCreateInfo MakeVkPipelineRasterizationStateCreateInfo()
	{
		VkPipelineRasterizationStateCreateInfo raster_info{};
		memset(&raster_info, 0, sizeof(raster_info));
		raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		return raster_info;
	}

	static inline VkPipelineMultisampleStateCreateInfo MakeVkPipelineMultisampleStateCreateInfo(uint32_t sample_count, bool use_alpha_coverage)
	{
		VkPipelineMultisampleStateCreateInfo ms_info{};
		memset(&ms_info, 0, sizeof(ms_info));
		ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		ms_info.rasterizationSamples = static_cast<VkSampleCountFlagBits>(sample_count);
		ms_info.alphaToCoverageEnable = use_alpha_coverage ? VK_TRUE : VK_FALSE;
		return ms_info;
	}

	static inline VkPipelineColorBlendStateCreateInfo MakeVkPipelineColorBlendStateCreateInfo()
	{
		VkPipelineColorBlendStateCreateInfo color_info{};
		memset(&color_info, 0, sizeof(color_info));
		color_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		return color_info;
	}

	static inline VkPipelineDynamicStateCreateInfo MakeVkPipelineDynamicStateCreateInfo(const VkDynamicState* states, const uint32_t count)
	{
		VkPipelineDynamicStateCreateInfo dynamic_info{};
		memset(&dynamic_info, 0, sizeof(dynamic_info));
		dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_info.dynamicStateCount = count;
		dynamic_info.pDynamicStates = states;
		return dynamic_info;
	}

	static inline VkPipelineBindPoint TransPipeTypeToBindPoint(VulkanRenderPipeline::EPipeType type)
	{
		using SrcType = VulkanRenderPipeline::EPipeType;
		switch (type)
		{
		case SrcType::eCompute:
			return VK_PIPELINE_BIND_POINT_COMPUTE;
		case SrcType::eGraphics:
			return VK_PIPELINE_BIND_POINT_GRAPHICS;
		case SrcType::eRayTrace:
			return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
		default:
			throw std::invalid_argument("no such bind point");
		}
	}

	VulkanRenderPipeline::Ptr VulkanRenderPipeline::Create(VulkanDevice::Ptr device, const VulkanRenderPipeline::Desc& desc)
	{
		VulkanRenderPipeline::Ptr pipe_handle;
		if (EPipeType::eCompute == desc.pipe_type_)
		{
			pipe_handle.reset(new VulkanComputePipeline(device, desc));
		}
		else if (EPipeType::eGraphics == desc.pipe_type_)
		{
			pipe_handle.reset(new VulkanGraphicsPipeline(device, desc));
		}
		else if (EPipeType::eRayTrace == desc.pipe_type_)
		{
			pipe_handle.reset(new VulkanRayTracingPipeline(device, desc));
		}
		else
		{
			throw std::invalid_argument("such pipe type not supported");
		}

		return pipe_handle;
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

	DescriptorSetsWrapper& VulkanRenderPipeline::operator[](const std::string& set_name)
	{
		return desc_sets_[desc_lut_[set_name]];
	}

	VulkanRenderPipeline::VulkanRenderPipeline(VulkanDevice::Ptr device, const Desc& desc, EPipeType pipe_type) : device_(device), pipe_type_(pipe_type)
	{
		//create pipeline layout
		auto& layout_info = MakePipelineLayoutCreateInfo(desc.ds_layouts_, desc.pc_range_);
		auto ret = vkCreatePipelineLayout(device_->Get(), &layout_info, g_host_alloc, &layout_);
		if (VK_SUCCESS != ret)
		{
			throw std::runtime_error("pipeline construction failed");
		}
	}

	VulkanRenderPipeline::~VulkanRenderPipeline()
	{
		if (VK_NULL_HANDLE != handle_)
		{
			vkDestroyPipeline(device_->Get(), handle_, g_host_alloc);
		}

		if (VK_NULL_HANDLE != layout_)
		{
			vkDestroyPipelineLayout(device_->Get(), layout_, g_host_alloc);
		}
	}

	void VulkanRenderPipeline::PushConsts(VulkanCmdBuffer& cmd_buffer, const uint32_t stage, const uint32_t offset, const Span<uint8_t>& values)
	{
		assert(offset & 0x3 == 0 && values.size() & 0x3 == 0 && "push constants should align to 4");
		vkCmdPushConstants(cmd_buffer.Get(), layout_, static_cast<VkShaderStageFlags>(stage), offset&~0x3, values.size()&~0x3, values.data());
	}

	VulkanComputePipeline::VulkanComputePipeline(VulkanDevice::Ptr device, const VulkanRenderPipeline::Desc& param) : VulkanRenderPipeline(device, EPipeType::eCompute)
	{
		VkComputePipelineCreateInfo pipeline_info;
		memset(&pipeline_info, 0, sizeof(pipeline_info));
		pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipeline_info.flags = param.flags_;
		pipeline_info.layout = layout_;
		//initial shaders
		const auto& compute_info = param.compute_;
		assert(compute_info.stage_.shader_type_ == VulkanShaderModule::EType::eCompute);
		auto compute_shader = std::make_shared<VulkanShaderModule>(new VulkanShaderModule(device, VulkanShaderModule::EType::eCompute,
																							compute_info.stage_.shader_path_, compute_info.stage_.shader_name_));
		pipeline_info.stage = MakeShaderStageCreateInfo(*compute_shader);
		stages_.clear();
		stages_.push_back(compute_shader);
		auto ret = vkCreateComputePipelines(device_->Get(), device_->GetPipelineCache(), 1, &pipeline_info, g_host_alloc, &handle_);
		if (VK_SUCCESS != ret)
		{
			throw std::runtime_error("compute pipeline contruct failed");
		}
	}

	VulkanGraphicsPipeline::VulkanGraphicsPipeline(VulkanDevice::Ptr device, const VulkanRenderPipeline::Desc& param) : VulkanRenderPipeline(device, EPipeType::eGraphics)
	{
		VkGraphicsPipelineCreateInfo pipeline_info;
		memset(&pipeline_info, sizeof(VkGraphicsPipelineCreateInfo), 1);
		pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.flags = param.flags_;
		pipeline_info.layout = layout_;
		auto& gfx_info = param.gfx_;
		pipeline_info.renderPass = gfx_info.pass_;
		pipeline_info.subpass = gfx_info.subpass_;

		SmallVector<VkPipelineShaderStageCreateInfo> stage_create_infos;
		stages_.clear();
		for (auto& shader_info:gfx_info.stages_)
		{
			auto& shader = std::make_shared<VulkanShaderModule>(new VulkanShaderModule(device, shader_info.shader_type_, shader_info.shader_path_,
																						shader_info.shader_name_));
			stages_.push_back(shader);
			stage_create_infos.emplace_back(MakeShaderStageCreateInfo(*shader));
		}
		pipeline_info.pStages = stage_create_infos.data();
		pipeline_info.stageCount = stage_create_infos.size();

		//init other params
		auto& input_vertex_info = MakeVkPipelineVertexInputStateCreateInfo(gfx_info.vertex_descs_, gfx_info.attribute_descs_);
		pipeline_info.pVertexInputState = &input_vertex_info;
		auto& assembly_info = MakeVkPipelineInputAssemblyStateCreateInfo(static_cast<VkPrimitiveTopology>(gfx_info.topology_));
		pipeline_info.pInputAssemblyState = &assembly_info;
		auto& viewpoint_info = MakeVkPipelineViewportStateCreateInfo(gfx_info.view_points_, gfx_info.scissors_);
		pipeline_info.pViewportState = &viewpoint_info;
		auto& msaa_info = MakeVkPipelineMultisampleStateCreateInfo(gfx_info.sample_count_, gfx_info.use_alpha_coverage_);
		pipeline_info.pMultisampleState = &msaa_info;
		auto& dynamic_info = MakeVkPipelineDynamicStateCreateInfo(gfx_info.dyn_stats_.data(), gfx_info.dyn_stats_.size());
		pipeline_info.pDynamicState = &dynamic_info;
		
		auto ret = vkCreateGraphicsPipelines(device_->Get(), device->GetPipelineCache(), 1, &pipeline_info, g_host_alloc, &handle_);
		if (VK_SUCCESS != ret)
		{
			throw std::runtime_error("graphics pipeline contruct failed");
		}
	}

	VulkanShaderModule::Ptr VulkanGraphicsPipeline::GetShader(VulkanShaderModule::EType shader_type)
	{
		for (auto& shader : stages_)
		{
			if (shader->Type() == shader_type)
			{
				return shader;
			}
		}
		throw std::runtime_error("no such type shader");
	}

	VulkanRayTracingPipeline::VulkanRayTracingPipeline(VulkanDevice::Ptr device, const VulkanRenderPipeline::Desc& param) : VulkanRayTracingPipeline(device, EPipeType::eRayTrace)
	{
		throw std::runtime_error("not implemented now");
	}

}