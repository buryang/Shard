#pragma once
#include "RHI/VulkanRHI.h"
#include "RHI/VulkanResource.h"
#include "RHI/VulkanRenderShader.h"
#include "RHI/VulkanFrameBuffer.h"
#include <unordered_map>

namespace MetaInit
{
	class VulkanCmdBuffer;
	class DescriptorSetsWrapper;
	class VulkanRenderPipeline
	{
	public:
		//todo: support task and mesh shader
		enum class EPipeType:uint8_t
		{
			eCompute,
			eGraphics,
			eRayTrace,
			eNum,
		};
		using Ptr = std::unique_ptr<VulkanRenderPipeline>;
		typedef struct _VulkanRenderPipelineDescs
		{
			EPipeType						pipe_type_;
			VkPipelineCreateFlags			flags_;
			//pipeline layout create info params
			PipelineLayout					layout_;
			typedef struct _VulkanShaderInfo
			{
				VulkanShaderModule::EType shader_type_;
				std::string				  shader_path_;
				std::string				  shader_name_;
			}ShaderDesc;
			typedef struct _VulkanRayTraceGroupInfo
			{
				uint32_t			group_type_{ VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR };
				uint32_t			general_{ 0 };
				uint32_t			close_hit_{ 0 };
				uint32_t			any_hit_{ 0 };
				uint32_t			intersect_{ 0 };
			}GroupDesc;
			typedef struct _VulkanBlendAttatchment
			{
				bool				blend_;
				uint8_t				color_op_;
				uint8_t				src_color_factor_;
				uint8_t				dst_color_factor_;
				uint8_t				alpha_op_;
				uint8_t				src_alpha_factor_;
				uint8_t				dst_alpha_factor_;
				uint8_t				color_mask_;
			}BlendDesc;
			union {
				struct {
					VkRenderPass			pass_{ VK_NULL_HANDLE };
					uint32_t				topology_{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
					uint32_t				subpass_;
					Vector<ShaderDesc>		stages_;
					//color blend arguments
					vec4					blend_const_;
					Vector<BlendDesc>		blend_attachs_;
					//rasterization arguments
					uint32_t				polygon_mode_{ VK_POLYGON_MODE_FILL };
					uint32_t				cull_mode_{ VK_CULL_MODE_BACK_BIT };
					uint32_t				front_face_{ VK_FRONT_FACE_COUNTER_CLOCKWISE };
					float					line_width_{ 1.0f};
					//multisample arguments
					uint32_t				sample_count_ = 1;
					bool					use_alpha_coverage_ = false;
					//viewpoint arguments
					Vector<VkViewport>		view_points_;
					Vector<VkRect2D>		scissors_;
					//depth stencil
					vec2					depth_bounds_;
					bool					depth_test_;
					bool					depth_write_;
					bool					depth_bound_test_;
					bool					stencil_test_;
					VkCompareOp				depth_compare_{ VK_COMPARE_OP_LESS };
					VkStencilOpState		front_;
					VkStencilOpState		back_;
					//dynamic state arguments
					//VkPipelineDynamicStateCreateInfo::pDynamicStates 
					//property of the currently active pipeline
					Vector<VkDynamicState>	dyn_stats_;
					//vertex attribution arguments
					Vector<VkVertexInputBindingDescription>		vertex_descs_;
					Vector<VkVertexInputAttributeDescription>	attribute_descs_;
				}gfx_;
				struct {
					ShaderDesc				stage_;
				}compute_;
				struct {
					//todo 
					Vector<ShaderDesc>		stages_;
					Vector<GroupDesc>		groups_;
					uint32_t				max_depth_{ 4 };
				}raytrace_;
			};
		}Desc;
		static Ptr Create(VulkanDevice::Ptr device, const Desc& desc_params);
		//pipeline resource 
		void Bind(VulkanCmdBuffer& cmd_buffer);
		//constant data 
		void PushConsts(VulkanCmdBuffer& cmd_buffer, const uint32_t stage, const uint32_t offset, const Span<uint8_t>& data);
		VkPipeline Get() { return handle_; }
		const VkPipelineLayout GetLayout()const { return layout_; }
		EPipeType Type()const { return pipe_type_; }
		DescriptorSetsWrapper& operator[](const std::string& set_name); 
		VulkanRenderPipeline(VulkanDevice::Ptr device, const Desc& desc, EPipeType pipe_type);
		virtual ~VulkanRenderPipeline();
	private:
		void InitialDescSetLayouts(const RootSignature& root);
	protected:
		using ShaderList = Vector<VulkanShaderModule::Ptr>;
		using DescSetList = Vector<DescriptorSetsWrapper>;
		VulkanDevice::Ptr				device_;
		VkPipeline						handle_{ VK_NULL_HANDLE };
		EPipeType						pipe_type_;
		VkPipelineLayout				layout_{ VK_NULL_HANDLE };
		Vector<VkDescriptorSetLayout>	ds_layouts_;
		//VkDescriptorUpdateTemplate		desc_template_{ VK_NULL_HANDLE };
		DescSetList						descs_;
		ShaderList						stages_;
	};

	
	class VulkanComputePipeline : public VulkanRenderPipeline
	{
	public:
		explicit VulkanComputePipeline(VulkanDevice::Ptr device, const VulkanRenderPipeline::Desc& param);
	};

	class VulkanGraphicsPipeline : public VulkanRenderPipeline
	{
	public:
		explicit VulkanGraphicsPipeline(VulkanDevice::Ptr device, const VulkanRenderPipeline::Desc& param);
		VulkanShaderModule::Ptr GetShader(VulkanShaderModule::EType shader_type);
		//bind vertex info
		void SetVAO(const VulkanVertexAttributes& vao);
		//dynamic state 
		void SetViewPoint(const VkViewport& view_point);
		void SetStencil(const uint32_t stentil_ref);
		void SetScissor(const VkRect2D& scissor);
		//init render pass and etc.
		void PrepareForDraw(VulkanCmdBuffer& cmd_buffer, VulkanFrameBuffer& frame_buffer);
		void EndForDraw(VulkanCmdBuffer& cmd_buffer);
	private:
		//only initial when pipeline has graph viewpoint info
		Optional<VkViewport>		view_point_;
		Optional<uint32_t>			stencil_ref_;
		Optional<VkRect2D>			scissor_;
		VulkanFrameBuffer			frame_buffer_;
		//fixed-function vertex process data

	};

	class VulkanRayTracingPipeline : public VulkanRenderPipeline
	{
	public:
		explicit VulkanRayTracingPipeline(VulkanDevice::Ptr device, const VulkanRenderPipeline::Desc& param);
	};

	VkPipelineCacheCreateInfo MakePipelineCacheCreateInfo(VkPipelineCacheCreateFlags flags, const Vector<uint8_t>& initial_data);
}
