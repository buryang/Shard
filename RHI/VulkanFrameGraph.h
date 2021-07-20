#pragma once
#include "Utils/CommonUtils.h"
#include "VulkanRHI.h"
#include <set>

namespace MetaInit
{
	class VulkanCmdPoolManager;
	class VulkanInstance;
	class VulkanDevice;
	class VulkanWindowSystemImpl;
	class VulkanVMAWrapper;

	class VulkanFrameContextGraph
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
		void Begin();
		void Draw();
		void End();
		void UpdataWSI();
	private:
		void CreateSwapChain();
	private:
		VulkanInstance::Ptr				instance_;
		VulkanDevice::Ptr				device_;
		VulkanCmdPoolManager::Ptr		cmd_pool_;
		VulkanWindowSystemImpl::Ptr		wsi_impl_;
		VulkanVMAWrapper::Ptr			vma_alloc_;
	};
}
