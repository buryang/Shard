#include "RHI/VulkanFrameGraph.h"

namespace MetaInit
{
	VulkanFrameContextGraph::Ptr VulkanFrameContextGraph::Clone() const
	{
		return VulkanFrameContextGraph::Ptr(new VulkanFrameContextGraph(*this));
		//todo other copy and assign work
	}

	VulkanFrameContextGraph::~VulkanFrameContextGraph()
	{

	}

	void VulkanFrameContextGraph::BeginBuild()
	{

	}

	void VulkanFrameContextGraph::Build(SceneProxyHelper& scene)
	{
		InitRenderResource();
		//multi thread build task here
		for (;;)
		{
			BuildSubTask();
		}
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

			auto queue = device_->GetQueue(VulkanDevice::EQueueType::eAllIn);
			queue.Submit(Span<VkCommandBuffer>(tmp_cmd_bufs.data(), tmp_cmd_bufs.size()), Span<VkSemaphore>(&image_available_, 1),
						Span<VkSemaphore>(&present_signal_, 1));

			//display
			wsi_impl_->SubmitFrameBufferAsync(present_signal_, 0);//fixme
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