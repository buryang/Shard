#pragma once
#include "RHI/VulkanRHI.h"
#include "GLFW/glfw3.h"
#include <atomic>

namespace MetaInit
{
	VkSwapchainCreateInfoKHR MakeSwapChainCreateInfo(VkSwapchainCreateFlagsKHR flags, 
													VkSurfaceKHR surface,
													const VkSurfaceCapabilitiesKHR& surface_capabilities,
													VkSwapchainKHR old_chain = VK_NULL_HANDLE);
	VkWin32SurfaceCreateInfoKHR MakeSurfaceCreateInfo(VkWin32SurfaceCreateFlagsKHR flags, HINSTANCE hinstance, HWND hwnd);
	VkFramebufferCreateInfo MakeFramebufferCreateInfo(VkFramebufferCreateFlags flags, VkRenderPass pass, Span<VkImageView>& attachs, 
													  uint32_t width, uint32_t height, uint32_t layers);

	class VulkanDevice;
	class VulkanInstance;
	class VulkanWindowSystemImpl
	{
	public:
		using Ptr = std::shared_ptr<VulkanWindowSystemImpl>;
		VulkanWindowSystemImpl(VulkanInstance::Ptr instance, VulkanDevice::Ptr device, GLFWwindow* window);
		VulkanWindowSystemImpl(VulkanWindowSystemImpl&& wsi);
		~VulkanWindowSystemImpl();
		void UpdateSwapChain(const VkSwapchainCreateInfoKHR& swap_info);
		VkFramebuffer GetFrameBufferAsync(VkSemaphore& semaphore, uint32_t& fbo_index);
		void SubmitFrameBufferAsync(const VkSemaphore semaphore, const uint32_t fbo_index);
		//check whether a vkimage is swap_images
		bool IsSwapChainImage(const VkImage image)const;
	private:
		void InitFBOs();
	private:
		VkSurfaceKHR					surface_{ VK_NULL_HANDLE };
		//VkSurfaceFormatKHR			surface_format_;
		VkSwapchainKHR					swap_chain_{ VK_NULL_HANDLE };
		VkFormat						swap_format_;
		VkExtent2D						swap_extent_;
		Vector<VkImage>					swap_images_;
		Vector<VkImageView>				swap_image_views_;
		Vector<VkSemaphore>				semaphore_acquires_;
		VulkanDevice::Ptr				device_;
		VulkanInstance::Ptr				instance_;
		GLFWwindow*						window_{ nullptr };
	};
}
