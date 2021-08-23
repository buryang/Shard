#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/VulkanRHI.h"
#include "RHI/VulkanResource.h"
#include "RHI/VulkanRenderPass.h"
#include "RHI/VulkanCmdContext.h"
#include "RHI/VulkanMemAllocator.h"
#include "RHI/VulkanWindowSystem.h"
#include <set>
#include <memory>

namespace MetaInit
{

	VkSemaphoreCreateInfo MakeSemphoreCreateInfo(VkSemaphoreCreateFlags flags = 0x0);

	class VulkanInstance;
	class VulkanDevice;
	class VulkanWindowSystemImpl;
	class VulkanRenderPass;

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
		VulkanWindowSystemImpl&			wsi_impl_;
		VulkanVMAWrapper&				vma_alloc_;
		DescriptorPoolManager::Ptr		desc_manager_;
		//VulkanRenderPass				render_pass_;
		VkSemaphore						image_available_{ VK_NULL_HANDLE };
		//ready for render
		VkSemaphore						present_signal_{ VK_NULL_HANDLE };
		VulkanRenderPass				render_pass_;
	};
}
