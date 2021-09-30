#include "Renderer/RtRenderGraph.h"

namespace MetaInit
{
	namespace Renderer
	{
		RtRendererFrameContextGraph::Ptr RtRendererFrameContextGraph::Clone() const
		{
			return RtRendererFrameContextGraph::Ptr(new RtRendererFrameContextGraph(*this));
			//todo other copy and assign work
		}

		RtRendererFrameContextGraph::~RtRendererFrameContextGraph()
		{
			if (VK_NULL_HANDLE != present_signal_)
			{

				vkDestroySemaphore(device_->Get(), present_signal_, g_host_alloc);
			}
		}

		void RtRendererFrameContextGraph::Build(SceneProxyHelper& scene)
		{
			InitRenderResource();
			//multi thread build task here
			for (;;)
			{
				BuildSubTask();
			}
		}

		void RtRendererFrameContextGraph::BuildSubTask()
		{
		}

		void RtRendererFrameContextGraph::EndBuild()
		{
		}

		void RtRendererFrameContextGraph::Execute()
		{
			if (!cmd_buffers_.empty())
			{
				SmallVector<VkCommandBuffer> tmp_cmd_bufs;
				for (auto& cmd : cmd_buffers_)
				{
					tmp_cmd_bufs.push_back(cmd->Get());
				}

				auto queue = device_->GetQueue(VulkanDevice::EQueueType::eAllIn);
				queue->Submit(Span<VkCommandBuffer>(tmp_cmd_bufs.data(), tmp_cmd_bufs.size()), Span<VkSemaphore>(&image_available_, 1),
					Span<VkSemaphore>(&present_signal_, 1));

				//display
				wsi_impl_->SubmitFrameBufferAsync(present_signal_, 0);//fixme
			}

		}

		RtRenderResource& RtRendererFrameContextGraph::GetTextureResource()
		{
			// TODO: 在此处插入 return 语句
		}

		RtRenderResource& RtRendererFrameContextGraph::GetBufferResource()
		{
			// TODO: 在此处插入 return 语句
		}

		void RtRendererFrameContextGraph::InitRenderResource()
		{
			//init framebuffer
			SmallVector<VkImageView> attachments;

			for (auto n = 0; n < device_->GetMaxColorTargetCount; ++n)
			{

			}

			auto& work_load = wsi_impl_->GetFrameBufferAsync(&image_available_);
		}
	}

}