#include "VulkanCmdContext.h"
#include "VulkanRenderPass.h"

namespace MetaInit
{
	VulkanCmdBuffer::Ptr VulkanCmdBuffer::Create(const Parameters& params, VulkanCmdPool& cmd_pool)
	{}

	VulkanCmdBuffer::~VulkanCmdBuffer()
	{
		Reset();
	}

	void VulkanCmdBuffer::Begin()
	{
		VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		begin_info.pNext = VK_NULL_HANDLE;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = VK_NULL_HANDLE;
		vkBeginCommandBuffer(buffer_, &begin_info);
	}

	void VulkanCmdBuffer::End()
	{
		vkEndCommandBuffer(buffer_);
	}

	void VulkanCmdBuffer::BeginPass(VulkanRenderPass& render_pass)
	{
		render_pass.Begin(*this);
	}

	void VulkanCmdBuffer::EndPass(VulkanRenderPass& render_pass)
	{
		render_pass.End(*this);
	}

	void VulkanCmdBuffer::Dispatch()
	{
		//FIXME
		vkCmdDispatch(buffer_,0, 0, 0);
	}

	void VulkanCmdBuffer::Submit(VulkanQueue& queue)
	{
		//
	}
}