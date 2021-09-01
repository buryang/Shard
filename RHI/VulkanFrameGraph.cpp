#include "VulkanFrameGraph.h"

namespace MetaInit
{

	VkSemaphoreCreateInfo MakeSemphoreCreateInfo(VkSemaphoreCreateFlags flags)
	{
		VkSemaphoreCreateInfo semaphore_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		semaphore_info.pNext = VK_NULL_HANDLE;
		semaphore_info.flags = flags;
		return semaphore_info;
	}

	VulkanFrameContextGraph::~VulkanFrameContextGraph()
	{

	}

	void VulkanFrameContextGraph::BeginBuild()
	{

	}

	void VulkanFrameContextGraph::BuildSubTask()
	{
	}

	void VulkanFrameContextGraph::EndBuild()
	{
	}

	void VulkanFrameContextGraph::Execute()
	{
		if (!cmd_buffers_.empty())
		{
			SmallVector<VkCommandBuffer> tmp_cmd_bufs;
			for (auto& cmd : cmd_buffers_)
			{
				tmp_cmd_bufs.push_back(cmd->Get());
			}
		}
	}

	void VulkanFrameContextGraph::InitRenderResource()
	{
		//init framebuffer
		SmallVector<VkImageView> attachments;

		for (auto n = 0; n < device_->GetMaxColorTargetCount; ++n)
		{
			
		}
	}

}