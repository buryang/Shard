#include "VulkanWindowSystem.h"
#include "RHI/VulkanRHI.h"

namespace MetaInit
{
	VkSwapchainCreateInfoKHR MakeSwapChainCreateInfo(VkSwapchainCreateFlagsKHR flags,
													VkSurfaceKHR surface,
													const SmallVector<uint32_t>& family_indices,
													VkSwapchainKHR old_swap)
	{
		VkSwapchainCreateInfoKHR swap_info{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		swap_info.pNext = VK_NULL_HANDLE;
		swap_info.queueFamilyIndexCount = family_indices.size();
		swap_info.pQueueFamilyIndices = family_indices.data();
		swap_info.oldSwapchain = old_swap;
		return swap_info;
	}

	VkWin32SurfaceCreateInfoKHR MakeSurfaceCreateInfo(VkWin32SurfaceCreateFlagsKHR flags, HINSTANCE hinstance, HWND hwnd)
	{
		VkWin32SurfaceCreateInfoKHR surface_info{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
		surface_info.pNext = VK_NULL_HANDLE;
		surface_info.flags = flags;
		surface_info.hinstance = hinstance;
		surface_info.hwnd = hwnd;
		return surface_info;
	}

	VulkanWindowSystemImpl::VulkanWindowSystemImpl(VulkanDevice device, const VkSwapchainCreateInfoKHR& swap_info)
	{
		auto ret = vkCreateSwapchainKHR(device.Get(), &swap_info, g_host_alloc, &swap_chain_);
		assert(ret == VK_SUCCESS && "create swapchain failed");
	}

	VulkanWindowSystemImpl::VulkanWindowSystemImpl(VulkanWindowSystemImpl&& wsi)
	{
		surface_ = wsi.surface_;
		swap_chain_ = wsi.swap_chain_;
		hdr_meta_ = wsi.hdr_meta_;
		wsi.surface_ = VK_NULL_HANDLE;
		wsi.swap_chain_ = VK_NULL_HANDLE;
	}
}