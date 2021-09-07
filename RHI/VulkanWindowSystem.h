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
		typedef struct _VulkanFrameWorkLoad
		{
			VkImage			image_{ VK_NULL_HANDLE };
			VkImageView		image_view_{ VK_NULL_HANDLE };
			VkFence			fence_{ VK_NULL_HANDLE };
			bool			using_ = false;
		}FrameWorkLoad;
		using Ptr = std::shared_ptr<VulkanWindowSystemImpl>;
		VulkanWindowSystemImpl(VulkanInstance::Ptr instance, VulkanDevice::Ptr device, GLFWwindow* window);
		VulkanWindowSystemImpl(VulkanWindowSystemImpl&& wsi);
		~VulkanWindowSystemImpl();
		void OnWindowChange(GLFWwindow* window);
		void OnWindowResize(uint32_t sizex, uint32_t sizey);
		void UpdateSwapChain(const VkSwapchainCreateInfoKHR& swap_info);
		//https://stackoverflow.com/questions/60419749/why-does-vkacquirenextimagekhr-never-block-my-thread
		//"image is not yet available to you"
		FrameWorkLoad GetFrameBufferAsync(VkSemaphore& semaphore);
		void SubmitFrameBufferAsync(VkSemaphore semaphore, const uint32_t index);
		//check whether a vkimage is swap_images
		bool IsSwapChainImage(const VkImage image)const;
	private:
		void InitFrameWorkLoads();
		void ResetFrameWorkLoads();
	private:
		VkSurfaceKHR					surface_{ VK_NULL_HANDLE };
		//VkSurfaceFormatKHR			surface_format_;
		VkSwapchainKHR					swap_chain_{ VK_NULL_HANDLE };
		VkFormat						swap_format_;
		VkExtent2D						swap_extent_;
		Vector<FrameWorkLoad>			frame_workloads_;
		Vector<VkSemaphore>				acquires_;
		VulkanDevice::Ptr				device_;
		VulkanInstance::Ptr				instance_;
		VulkanQueue::Ptr				present_queue_;
		GLFWwindow*						window_{ nullptr };
		uint32_t						acquire_index_ = -1;
		//critical section resouce here
		std::mutex						mutex_;
	};
}
