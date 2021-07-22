#pragma once
#include "VulkanRHI.h"

namespace MetaInit
{
	VkSwapchainCreateInfoKHR MakeSwapChainCreateInfo(VkSwapchainCreateFlagsKHR flags, 
													VkSurfaceKHR surface,
													const SmallVector<uint32_t>& family_indices,
													VkSwapchainKHR old_chain = VK_NULL_HANDLE);
	VkWin32SurfaceCreateInfoKHR MakeSurfaceCreateInfo(VkWin32SurfaceCreateFlagsKHR flags, HINSTANCE hinstance, HWND hwnd);

	class VulkanDevice;
	class VulkanWindowSystemImpl
	{
	public:
		using Ptr = std::shared_ptr<VulkanWindowSystemImpl>;
		VulkanWindowSystemImpl(VulkanDevice::Ptr device, const VkSwapchainCreateInfoKHR& swap_info);
		VulkanWindowSystemImpl(VulkanWindowSystemImpl&& wsi);
		~VulkanWindowSystemImpl();
		void UpdateSwapChain(const VkSwapchainCreateInfoKHR& swap_info);
		VkSurfaceKHR Get() { return surface_; };
	private:
		VkSurfaceKHR					surface_{ VK_NULL_HANDLE };
		VkSurfaceCapabilitiesKHR		capabilities_;
		VkSurfaceFormatKHR				surface_format_;
		VkSwapchainKHR					swap_chain_{ VK_NULL_HANDLE };
		VkHdrMetadataEXT				hdr_meta_;
	};
}
