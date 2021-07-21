#include "VulkanRenderPass.h"
#include "VulkanCmdContext.h"
#include <array>

namespace
{
	constexpr auto max_render_targets = 9;
	constexpr auto max_subpass_count  = 8;
}

namespace MetaInit {

	VkRenderPassCreateInfo MakeRenderPassCreateInfo(VkRenderPassCreateFlags flags, Vector<VkAttachmentDescription>& attach_descs,
		Vector<VkSubpassDescription>& subpass_descs, Vector<VkSubpassDependency>& subpass_deps)
	{
		VkRenderPassCreateInfo pass_info{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		pass_info.flags = flags;
		pass_info.pNext = VK_NULL_HANDLE;
		if (!attach_descs.empty())
		{
			pass_info.attachmentCount = attach_descs.size();
			pass_info.pAttachments = attach_descs.data();
		}
		else
		{
			pass_info.attachmentCount = 0;
			pass_info.pAttachments = VK_NULL_HANDLE;
		}

		if (!subpass_descs.empty())
		{
			pass_info.subpassCount = subpass_descs.size();
			pass_info.pSubpasses = subpass_descs.data();
		}
		else
		{
			pass_info.subpassCount = 0;
			pass_info.pSubpasses = VK_NULL_HANDLE;
		}

		if (!subpass_deps.empty())
		{
			pass_info.dependencyCount = subpass_deps.size();
			pass_info.pDependencies = subpass_deps.data();
		}
		else
		{
			pass_info.dependencyCount = 0;
			pass_info.pDependencies = VK_NULL_HANDLE;
		}
		return pass_info;

	}

	static VkRenderPass create_render_pass(VkDevice device, VkRenderPassCreateInfo pass_info)
	{
		std::array<VkAttachmentDescription, max_render_targets> attach_descs;
		std::array<VkAttachmentReference, max_render_targets> attach_refs;
		std::array<VkSubpassDescription, max_subpass_count> subpass_descs;
		std::array<VkSubpassDependency, max_subpass_count> subpass_deps;

		const auto subpass_count = pass_info.subpassCount;

		for (auto n = 0; n < subpass_count; ++n)
		{
			auto& subpass_desc= subpass_descs[n];
			subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass_desc.colorAttachmentCount = 0;
			subpass_desc.pColorAttachments = attach_refs.data();
			subpass_desc.inputAttachmentCount = 0;
			subpass_desc.pInputAttachments = VK_NULL_HANDLE;
			subpass_desc.pDepthStencilAttachment = VK_NULL_HANDLE;

			auto& subpass_dep = subpass_deps[n];
			subpass_dep.srcSubpass = n == 0 ? VK_SUBPASS_EXTERNAL : n - 1;
			subpass_dep.dstSubpass = n;
			subpass_dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			subpass_dep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			subpass_dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			subpass_dep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		}

		//VkRenderPassCreateInfo pass_info{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		pass_info.flags = 1;
		pass_info.subpassCount = subpass_count;
		pass_info.pSubpasses = subpass_descs.data();
		pass_info.dependencyCount = subpass_count;
		pass_info.pDependencies = subpass_deps.data();
		pass_info.attachmentCount = 0;
		pass_info.pAttachments = VK_NULL_HANDLE;
		pass_info.pNext = VK_NULL_HANDLE;
		VkRenderPass render_pass = VK_NULL_HANDLE;
		vkCreateRenderPass(device, &pass_info, nullptr, &render_pass);
		return render_pass;
	}

	VulkanRenderPass::~VulkanRenderPass()
	{
		vkDestroyRenderPass(device_->Get(), pass_, g_host_alloc);
	}

	void VulkanRenderPass::Begin(VulkanCmdBuffer& cmd)
	{
		VkRenderPassBeginInfo begin_info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
		begin_info.pNext = VK_NULL_HANDLE;
		begin_info.framebuffer = nullptr;
		begin_info.pClearValues = nullptr;
		//begin_info.renderArea = nullptr;
		begin_info.renderPass = pass_;
		vkCmdBeginRenderPass(cmd.Get(), &begin_info, VK_SUBPASS_CONTENTS_INLINE);
	}

	void VulkanRenderPass::End(VulkanCmdBuffer& cmd)
	{
		vkCmdEndRenderPass(cmd.Get());
	}
}
