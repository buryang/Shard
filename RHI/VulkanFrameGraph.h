#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/VulkanRHI.h"
#include "RHI/VulkanRenderPass.h"
#include <set>
#include <memory>

namespace MetaInit
{

	VkSemaphoreCreateInfo MakeSemphoreCreateInfo(VkSemaphoreCreateFlags flags);

	class VulkanCmdPoolManager;
	class DescriptorPoolManager;
	class VulkanInstance;
	class VulkanDevice;
	class VulkanWindowSystemImpl;
	class VulkanVMAWrapper;

	class VulkanFrameContextGraph: public std::enable_shared_from_this<VulkanFrameContextGraph>
	{
	public:
		using Ptr = std::shared_ptr<VulkanFrameContextGraph>;
		VulkanFrameContextGraph(VulkanInstance::Ptr instance,
								VulkanDevice::Ptr device,
								VulkanCmdPoolManager::Ptr cmd_pool);
		DISALLOW_COPY_AND_ASSIGN(VulkanFrameContextGraph);
		~VulkanFrameContextGraph();
		VulkanInstance::Ptr GetInstance() { return instance_; }
		VulkanDevice::Ptr GetDevice() { return device_; }
		VulkanCmdPoolManager::Ptr GetPoolManager() { return cmd_pool_; }
		VulkanVMAWrapper::Ptr GetMemeAllocator() { return vma_alloc_; }
		void BeginBuild();
		void BuildSubTask();
		void EndBuild();
		void Execute();
		Ptr Get() { return shared_from_this(); }
	private:
		void InitRenderResource();
	private:
		VulkanInstance::Ptr				instance_;
		VulkanDevice::Ptr				device_;
		VulkanCmdPoolManager::Ptr		cmd_pool_;
		VulkanCmdBuffer::Ptr			cmd_buffer_;
		VulkanWindowSystemImpl::Ptr		wsi_impl_;
		VulkanVMAWrapper::Ptr			vma_alloc_;
		DescriptorPoolManager::Ptr		desc_manager_;
		//VulkanRenderPass				render_pass_;
		VkSemaphore						image_available_;
		//ready for render
		VkSemaphore						present_signal_;
		VulkanRenderPass				render_pass_;
	};
}
