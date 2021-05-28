#pragma once
#include "VulkanRHI.h"

namespace MetaInit
{
	class VulkanCmdBuffer;
	//interface for vulkan render pass
	class VulkanRenderPass
	{
	public:
		using Ptr = std::shared_ptr<VulkanRenderPass>;
		static Ptr Create();
		VkRenderPass Get() { return pass_; }
		void Begin(VulkanCmdBuffer& cmd);
		void End(VulkanCmdBuffer& cmd);
		~VulkanRenderPass();
	private:
		VulkanDevice::Ptr		device_;
		VkRenderPass				pass_ = VK_NULL_HANDLE;
		VkRenderPassBeginInfo	info_;
	};
}
