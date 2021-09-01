#pragma once
#include "RHI/VulkanRHI.h"
#include "RHI/VulkanFrameBuffer.h"

namespace MetaInit
{
	VkRenderPassBeginInfo MakeRenderPassBeginInfo(VkRenderPass render_pass, VkFramebuffer frame_buffer, VkRect2D render_area, Span<VkClearValue> clear_values);

	class VulkanSubRenderPass
	{
	public:
		VulkanSubRenderPass();
	private:
		friend class VulkanRenderPass;
		VkSubpassDescription	desc_;
		VkSubpassDependency		depends_;
	};

	class VulkanImageView;
	class VulkanAttachment
	{
	public:
		explicit VulkanAttachment(VulkanImageView& attach);
	private:
		VulkanImageView& attach_;
	};

	class VulkanCmdBuffer;
	//interface for vulkan render pass
	class VulkanRenderPass
	{
	public:
		typedef struct _VulkanRenderPassDesc
		{
			enum ELoadAction :uint8_t
			{
				eNoAction,
				eLoad,
				eClear,
			};
			enum ESaveAction :uint8_t
			{
				eNoAction,
				eStore,
				eResolve,
			};
			enum ETargetAction
			{
				eLoadOpMask = 2,
#define RTACTION_MAKE_MASK(load, store) ((static_cast<uint8_t>(load)<<static_cast<uint8_t>(eLoadOpMask))|static_cast<uint8_t>(store))
				eNoAction_NoAction = RTACTION_MAKE_MASK(ELoadAction::eNoAction, ESaveAction::eNoAction),
				eNoAction_Store = RTACTION_MAKE_MASK(ELoadAction::eNoAction, ESaveAction::eStore),
				eClear_Store = RTACTION_MAKE_MASK(ELoadAction::eClear, ESaveAction::eStore),
				eLoad_Store = RTACTION_MAKE_MASK(ELoadAction::eLoad, ESaveAction::eStore),
				eClear_NoAction = RTACTION_MAKE_MASK(ELoadAction::eClear, ESaveAction::eNoAction),
				eLoad_NoAction = RTACTION_MAKE_MASK(ELoadAction::eLoad, ESaveAction::eNoAction),
				eClear_Resolve = RTACTION_MAKE_MASK(ELoadAction::eClear, ESaveAction::eResolve),
				eLoad_Resolve = RTACTION_MAKE_MASK(ELoadAction::eLoad, ESaveAction::eResolve),
			};

			struct TargetDesc
			{
				VkFormat					format_{ VK_FORMAT_UNDEFINED };
				bool						read_only_{ false };
				ETargetAction				action_;
			};
			Vector<TargetDesc>	color_targets_;
			TargetDesc			depth_target_;
			uint32_t			width_;
			uint32_t			height_;
			uint32_t			layers_;

			//subpass part
			struct SubpassDesc
			{
				struct RefDesc
				{
					uint32_t			attach_idex_{ 0 };
					VkImageLayout		layout_;
				};
				Vector<RefDesc>		input_attachs_;
				Vector<RefDesc>		color_attachs_;
				Optional<RefDesc>	depth_attach_;
				Optional<RefDesc>	resolve_attach_;
			};
			Vector<SubpassDesc>		subpass_descs_;
			struct SubpassDepend
			{
				uint32_t			src_index_;
				uint32_t			dst_index_;
				uint32_t			src_stage_mask_;
				uint32_t			dst_stage_mask_;
				uint32_t			src_access_mask_;
				uint32_t			dst_access_mask_;
				uint32_t			flags_;
			};
			Vector<SubpassDepend>	subpass_depends_;
		}Desc;
		using Ptr = std::shared_ptr<VulkanRenderPass>;
		static Ptr Create(VulkanDevice::Ptr device, const Desc& desc);
		VkRenderPass Get() { return handle_; }
		void Begin(VulkanCmdBuffer& cmd, VulkanFrameBuffer& frame_buffer);
		void End(VulkanCmdBuffer& cmd);
		~VulkanRenderPass();
		VulkanRenderPass() = default;
		DISALLOW_COPY_AND_ASSIGN(VulkanRenderPass);
		/*property functions*/
		const uint32_t NumColorAttachments()const;
		VkFormat GetColorAttachMentFormat(uint32_t index)const;
		uint32_t GetColorAttachMentSampleCount(uint32_t index)const;
	private:
		void Init(VulkanDevice::Ptr device, const Desc& desc);
	private:
		VulkanDevice::Ptr				device_;
		VkRenderPass					handle_{ VK_NULL_HANDLE };		
		Desc							desc_;
	};
}
