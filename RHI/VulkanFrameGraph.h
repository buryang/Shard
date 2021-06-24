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

	class VulkanFrameContextGraph
	{
	public:
		VulkanFrameContextGraph(VulkanInstance& instance,
								VulkanDevice& device,
								VulkanCmdPoolManager& cmd_pool);
		DISALLOW_COPY_AND_ASSIGN(VulkanFrameContextGraph);
		~VulkanFrameContextGraph();
		VulkanInstance& GetInstance();
		VulkanDevice& GetDevice();
		VulkanCmdPoolManager& GetPoolManager();
		void Begin();
		void Draw();
		void End();
		void UpdataWSI();
	private:
		void CreateSwapChain();
	private:
		VulkanInstance&				instance_;
		VulkanDevice&				device_;
		VulkanCmdPoolManager&		cmd_pool_;
		VulkanWindowSystemImpl&		wsi_impl_;
	};
}
