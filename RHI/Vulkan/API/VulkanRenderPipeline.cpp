#include "Utils/CommonUtils.h"
#include "RHI/Vulkan/API/VulkanRenderPipeline.h"
#include "RHI/Vulkan/API/VulkanCmdContext.h"


namespace MetaInit
{

	VkPipelineCacheCreateInfo MakePipelineCacheCreateInfo(const Span<uint8_t>& initial_data)
	{
		VkPipelineCacheCreateInfo cache_info{};
		memset(&cache_info, 0, sizeof(VkPipelineCacheCreateInfo));
		cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		cache_info.initialDataSize = initial_data.size();
		cache_info.pInitialData = initial_data.data();
		return cache_info;
	}

	static inline VkPipelineLayoutCreateInfo MakePipelineLayoutCreateInfo(const Span<VkPushConstantRange>& push_range, const Span<VkDescriptorSetLayout>& ds_layout)
	{
		VkPipelineLayoutCreateInfo layout_info;
		memset(&layout_info, 0, sizeof(VkPipelineLayoutCreateInfo));
		layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
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

	static inline VkPipelineRasterizationStateCreateInfo MakeVkPipelineRasterizationStateCreateInfo(uint32_t poly_mode, uint32_t cull_mode, uint32_t front_face, float line_width=1.f)
	{
		VkPipelineRasterizationStateCreateInfo raster_info{};
		memset(&raster_info, 0, sizeof(raster_info));
		raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		raster_info.polygonMode = static_cast<VkPolygonMode>(poly_mode);
		raster_info.cullMode = static_cast<VkCullModeFlags>(cull_mode);
		raster_info.frontFace = static_cast<VkFrontFace>(front_face);
		raster_info.lineWidth = line_width;
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
		VkPipelineDynamicStateCreateInfo dyn_info{};
		memset(&dyn_info, 0, sizeof(dyn_info));
		dyn_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dyn_info.dynamicStateCount = count;
		dyn_info.pDynamicStates = states;
		return dyn_info;
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
			LOG(ERROR) << "no such bind point";
		}
	}

	VulkanRenderPipeline::Ptr VulkanRenderPipeline::Create(VulkanDevice::Ptr device, const VulkanRenderPipeline::Desc& desc)
	{
		VulkanRenderPipeline::Ptr pipe_handle;
		if (VulkanRenderPipeline::EPipeType::eCompute == desc.pipe_type_)
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
		for (auto& desc : descs_)
		{
			bind_sets.emplace_back(desc.Get());
		}
		vkCmdBindDescriptorSets(cmd_buffer.Get(), bind_point, layout_, 0, bind_sets.size(), bind_sets.data(), 0, nullptr);
	}

	VulkanDescriptorSet& VulkanRenderPipeline::operator[](const std::string& set_name)
	{
		auto iter = std::find_if(descs_.begin(), descs_.end(), [&](const auto& pred) {return pred.Name() == set_name;});
		if (descs_.end() != iter)
		{
			return *iter;
		}
		LOG(ERROR) << "wrong descriptor name";
	}

	VulkanRenderPipeline::VulkanRenderPipeline(VulkanDevice::Ptr device, const Desc& desc, EPipeType pipe_type) : device_(device), pipe_type_(pipe_type)
	{
		//initial descriptor set layout
		const auto& root_signature = desc.root_signatue_;
		InitialDescSetLayouts(root_signature);
		Vector<VkPushConstantRange> const_range(root_signature.consts_.size());
		for (auto n = 0; n < const_range.size(); ++n)
		{
			assert(root_signature.consts_[n].IsValid());
			const_range[n] = {root_signature.consts_[n].flags_, root_signature.consts_[n].offset_, root_signature.consts_[n].size_};
		}
		//create pipeline layout
		auto& layout_info = MakePipelineLayoutCreateInfo(const_range, ds_layouts_);
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

		for (auto& ds_layout : ds_layouts_)
		{
			if (VK_NULL_HANDLE != ds_layout)
			{
				vkDestroyDescriptorSetLayout(device_->Get(), ds_layout, g_host_alloc);
			}
		}
	}

	void VulkanRenderPipeline::InitialDescSetLayouts(const RootSignature& root)
	{
		ds_layouts_.clear();
		for (auto n = 0; n < root.GetNumDescriptorSets(); ++n)
		{
			const auto& set_cfg = root.GetDescriptorSetDesc(n);
			SmallVector<VkDescriptorSetLayoutBinding> binds;
			for (auto m = 0; m < set_cfg.Size(); ++m)
			{
				VkDescriptorSetLayoutBinding bind{};
				bind.binding = set_cfg.bindings_[m].binding_;
				bind.descriptorCount = set_cfg.bindings_[m].desc_count_;
				bind.descriptorType = static_cast<VkDescriptorType>(set_cfg.bindings_[m].desc_type_);
				bind.stageFlags = set_cfg.bindings_[m].stage_flags_;
				binds.emplace_back(bind);
			}

			auto& set_create_info = MakeDescriptorSetLayoutCreateInfo(0, binds);
			VkDescriptorSetLayout set_handle;
			vkCreateDescriptorSetLayout(device_->Get(), &set_create_info, g_host_alloc, &set_handle);
			ds_layouts_.push_back(set_handle);
		}

		//create descriptorset
		auto& ds_alloc_info = MakeDescriptorSetAllocateInfo(nullptr, ds_layouts_.data(), ds_layouts_.size());
		SmallVector<VkDescriptorSet> ds_buffer(ds_layouts_.size());
		vkAllocateDescriptorSets(device_->Get(), &ds_alloc_info, ds_buffer.data());
		descs_.resize(ds_buffer.size());
		for (auto n = 0; n < root.GetNumDescriptorSets(); ++n)
		{
			descs_[n].Init(root.GetDescriptorSetDesc(n), ds_buffer[n]);
		}

	}

	void VulkanRenderPipeline::PushConsts(VulkanCmdBuffer& cmd_buffer, const uint32_t stage, const uint32_t offset, const Span<uint8_t>& values)
	{
		assert(offset & 0x3 == 0 && values.size() & 0x3 == 0 && "push constants should align to 4");
		vkCmdPushConstants(cmd_buffer.Get(), layout_, static_cast<VkShaderStageFlags>(stage), offset&~0x3, values.size()&~0x3, values.data());
	}

	VulkanComputePipeline::VulkanComputePipeline(VulkanDevice::Ptr device, const VulkanRenderPipeline::Desc& param) : VulkanRenderPipeline(device, param, EPipeType::eCompute)
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
			//The stage member of each element of pStages must not be VK_SHADER_STAGE_COMPUTE_BIT
			assert(shader_info.shader_type_ != VulkanShaderModule::EType::eCompute);
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
		//pRasterizationState must be a valid pointer to a valid VkPipelineRasterizationStateCreateInfo structure
		auto& raster_info = MakeVkPipelineRasterizationStateCreateInfo(gfx_info.polygon_mode_, gfx_info.cull_mode_, gfx_info.front_face_, gfx_info.line_width_);
		pipeline_info.pRasterizationState = &raster_info;
		auto& viewpoint_info = MakeVkPipelineViewportStateCreateInfo(gfx_info.view_points_, gfx_info.scissors_);
		pipeline_info.pViewportState = &viewpoint_info;
		if (!gfx_info.view_points_.empty())
		{
			view_point_.emplace(gfx_info.view_points_[0]);
		}
		if (!gfx_info.scissors_.empty())
		{
			scissor_.emplace(gfx_info.scissors_[0]);
		}
		//todo depth 
		auto& msaa_info = MakeVkPipelineMultisampleStateCreateInfo(gfx_info.sample_count_, gfx_info.use_alpha_coverage_);
		pipeline_info.pMultisampleState = &msaa_info;

		VkPipelineDynamicStateCreateInfo dynamic_info;
		if (!gfx_info.dyn_stats_.empty())
		{
			InitialVkPipelineDynamicStateCreateInfo(dynamic_info, gfx_info.dyn_stats_.data(), gfx_info.dyn_stats_.size());
			pipeline_info.pDynamicState = &dynamic_info;
		}
		
		auto ret = vkCreateGraphicsPipelines(device_->Get(), param.pipeline_cache_, 1, &pipeline_info, g_host_alloc, &handle_);
		PCHECK(VK_SUCCESS != ret) << "graphics pipeline contruct failed";
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

	void VulkanGraphicsPipeline::PrepareForDraw(VulkanCmdBuffer& cmd_buffer, VulkanFrameBuffer& frame_buffer)
	{
		//other work

		//init render pass
		frame_buffer_ = frame_buffer;
		frame_buffer_.GetPass().Begin(cmd_buffer, frame_buffer);

		if(desc_.)
	}

	void VulkanGraphicsPipeline::EndForDraw(VulkanCmdBuffer& cmd_buffer)
	{
		frame_buffer_.GetPass().End(cmd_buffer);
	}

	void VulkanGraphicsPipeline::SetViewPoint(VulkanCmdBuffer& cmd_buffer, const VkViewport& view_point)
	{
		if (!view_point_.has_value() || std::memcmp(&view_point, &*view_point_, sizeof(view_point)) != 0)
		{
			vkCmdSetViewport(cmd_buffer.Get(), 0, 1, &view_point);
			view_point_ = view_point;
		}
	}

	void VulkanGraphicsPipeline::SetStencil(VulkanCmdBuffer& cmd_buffer, const uint32_t stentil_ref)
	{
		if (!stencil_ref_.has_value() || stencil_ref_.value() != stentil_ref)
		{
			vkCmdSetStencilReference(cmd_buffer.Get(), VK_STENCIL_FRONT_AND_BACK, stentil_ref);
			stencil_ref_ = stentil_ref;
		}
	}

	void VulkanGraphicsPipeline::SetScissor(VulkanCmdBuffer& cmd_buffer, const VkRect2D& scissor)
	{
		if (!scissor_.has_value() || std::memcmp(&scissor, &*scissor_, sizeof(scissor)) != 0)
		{
			vkCmdSetScissor(cmd_buffer.Get(), 0, 1, &scissor);
			scissor_ = scissor;
		}
	}

	VulkanRayTracingPipeline::VulkanRayTracingPipeline(VulkanDevice::Ptr device, const VulkanRenderPipeline::Desc& param) : VulkanRayTracingPipeline(device, EPipeType::eRayTrace)
	{
		throw std::runtime_error("not implemented now");
	}

}