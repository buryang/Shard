#pragma once
#include "RHI/VulkanRHI.h"
#include "RHI/VulkanFrameBuffer.h"

namespace MetaInit
{

	VkRenderPassCreateInfo MakeRenderPassCreateInfo(VkRenderPassCreateFlags flags, Vector<VkAttachmentDescription>& attach_descs,
													Vector<VkSubpassDescription>& subpass_descs, Vector<VkSubpassDependency>& subpass_deps);


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
		using Ptr = std::shared_ptr<VulkanRenderPass>;
		static Ptr Create(VulkanDevice::Ptr device, VulkanFrameBuffer frame_buffer, const VkRenderPassCreateInfo& pass_info);
		VkRenderPass Get() { return handle_; }
		void Begin(VulkanCmdBuffer& cmd);
		void End(VulkanCmdBuffer& cmd);
		void AddSubPass(VulkanSubRenderPass&& sub_pass);
		~VulkanRenderPass();
	private:
		VulkanDevice::Ptr		device_;
		VkRenderPass			handle_{ VK_NULL_HANDLE };
		VkRenderPassBeginInfo	info_;
		VulkanFrameBuffer				fbo_;			
		Vector<VulkanSubRenderPass>		sub_passes_;
		Vector<VkAttachmentDescription> attachments_;
	};
}
