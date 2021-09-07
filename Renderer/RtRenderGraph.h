#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/VulkanRHI.h"
#include "RHI/VulkanResource.h"
#include "RHI/VulkanRenderPass.h"
#include "RHI/VulkanRenderPipeline.h"
#include "RHI/VulkanCmdContext.h"
#include "RHI/VulkanMemAllocator.h"
#include "RHI/VulkanWindowSystem.h"
#include <set>
#include <memory>

namespace MetaInit
{
	class MINIT_API RenderFrameContextGraph : public std::enable_shared_from_this<RenderFrameContextGraph>
	{
	public:
		using Ptr = std::shared_ptr<RenderFrameContextGraph>;
		RenderFrameContextGraph(VulkanInstance::Ptr instance,
			VulkanDevice::Ptr device);
		RenderFrameContextGraph::Ptr Clone()const;
		virtual ~RenderFrameContextGraph();
		VulkanInstance::Ptr GetInstance() { return instance_; }
		VulkanDevice::Ptr GetDevice() { return device_; }
		VulkanVMAWrapper::Ptr GetMemeAllocator() { return vma_alloc_; }
		virtual void Build(SceneProxyHelper& scene);
		void Execute();
		Ptr Get() { return shared_from_this(); }
	private:
		RenderFrameContextGraph(const RenderFrameContextGraph& graph);
		RenderFrameContextGraph& operator=(const RenderFrameContextGraph& graph);
		void InitRenderResource();
		void BuildSubTask();
	private:
		VulkanInstance::Ptr				instance_;
		VulkanDevice::Ptr				device_;
		//fixme render pass relative to framebuffer and pipeline
		VulkanRenderPass::Ptr			render_pass_;
		Vector<VulkanRenderPipeline::Ptr>	render_pipelines_;
		//a frame use multi cmdbuffer to build 
		Vector<VulkanCmdBuffer::Ptr>	cmd_buffers_;
		VulkanWindowSystemImpl::Ptr		wsi_impl_;
		VulkanVMAWrapper::Ptr			vma_alloc_;
		//ready for pipeline to render
		VkSemaphore						image_available_{ VK_NULL_HANDLE };
		//ready for wsi to present
		VkSemaphore						present_signal_{ VK_NULL_HANDLE };
	};
}
