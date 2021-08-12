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