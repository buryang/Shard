#include "RHI/VulkanWindowSystem.h"
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

	VkFramebufferCreateInfo MakeFramebufferCreateInfo(VkFramebufferCreateFlags flags, VkRenderPass pass, Span<VkImageView>& attachs, uint32_t width, uint32_t height, uint32_t layers)
	{
		VkFramebufferCreateInfo frame_info{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		memset(&frame_info, 0, sizeof(frame_info));
		frame_info.attachmentCount = attachs.size();
		frame_info.pAttachments = attachs.data();
		frame_info.renderPass = pass;
		frame_info.width = width;
		frame_info.height = height;
		frame_info.layers = layers;
		return frame_info;
	}

	VulkanWindowSystemImpl::VulkanWindowSystemImpl(VulkanDevice::Ptr device, const VkWin32SurfaceCreateInfoKHR surface_info, const VkSwapchainCreateInfoKHR& swap_info)
	{
		//first create a surface
		auto ret = vkCreateWin32SurfaceKHR(nullptr, &surface_info, g_host_alloc, &surface_);
		if (VK_SUCCESS != ret)
		{
			throw std::runtime_error("failed to create windows surface");
		}
		
		ret = vkCreateSwapchainKHR(device->Get(), &swap_info, g_host_alloc, &swap_chain_);
		if (VK_SUCCESS != ret)
		{
			vkDestroySurfaceKHR(nullptr, surface_, g_host_alloc);
			throw std::runtime_error("failed to create swapchain");
		}
		InitFBOs();
	
	}

	VulkanWindowSystemImpl::VulkanWindowSystemImpl(VulkanWindowSystemImpl&& wsi)
	{
		surface_ = wsi.surface_;
		swap_chain_ = wsi.swap_chain_;
		hdr_meta_ = wsi.hdr_meta_;
		wsi.surface_ = VK_NULL_HANDLE;
		wsi.swap_chain_ = VK_NULL_HANDLE;
		InitFBOs();
	}

	VulkanWindowSystemImpl::~VulkanWindowSystemImpl()
	{
		if (VK_NULL_HANDLE != surface_)
		{
			vkDestroySurfaceKHR(nullptr, surface_, g_host_alloc);
		}

		if (VK_NULL_HANDLE != swap_chain_)
		{
			vkDestroySwapchainKHR(device_->Get(), swap_chain_, g_host_alloc);
		}

		for (auto& image : swap_images_)
		{
			vkDestroyImage(device_->Get(), image, g_host_alloc);	
		}

		for (auto& view : swap_image_views_)
		{
			vkDestroyImageView(device_->Get(), view, g_host_alloc);
		}
	}

	VkFramebuffer VulkanWindowSystemImpl::GetFrameBufferAsync(VkSemaphore& semaphore, uint32_t& fbo_index)
	{
		uint32_t image_index;
		vkAcquireNextImageKHR(device_->Get(), swap_chain_, std::numeric_limits<uint64_t>::max(),
								nullptr, nullptr, &image_index);

		//other sync work
		
	}

	void VulkanWindowSystemImpl::SubmitFrameBufferAsync(const VkSemaphore semaphore, const uint32_t fbo_index)
	{
		VkPresentInfoKHR present_info{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = &semaphore;
		present_info.swapchainCount = 1;
		present_info.pSwapchains = &swap_chain_;
		present_info.pImageIndices = &fbo_index;
		present_info.pResults = VK_NULL_HANDLE;

		auto query_surface_queue = [&](VkPhysicalDevice device, uint32_t family_index) {
			VkBool32 present_able = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, family_index, surface_, &present_able);
			return present_able == VK_TRUE;
		};

		auto& present_queue = device_->QueryQueue(query_surface_queue);
		present_queue.Submit(present_info);
	}

	void VulkanWindowSystemImpl::InitFBOs()
	{
		if (VK_NULL_HANDLE != swap_chain_)
		{
			uint32_t image_count = 0;
			vkGetSwapchainImagesKHR(device_->Get(), swap_chain_, &image_count, nullptr);
			assert(image_count > 0 && "swapchain should has images");
			swap_images_.resize(image_count);
			vkGetSwapchainImagesKHR(device_->Get(), swap_chain_, &image_count, swap_images_.data());

			//other post work
			swap_image_views_.resize(image_count);
			VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			memset(&view_info, 0, sizeof(view_info));
			view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view_info.format = swap_format_;
			view_info.subresourceRange.levelCount = 1;
			view_info.subresourceRange.layerCount = 1;
			for (auto n = 0; n < image_count; ++n)
			{
				view_info.image = swap_images_[n];
				vkCreateImageView(device_->Get(), &view_info, g_host_alloc, &swap_image_views_[n]);
			}
		}
	}
}