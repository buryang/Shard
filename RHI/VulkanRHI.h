#pragma once
#include "Utils/CommonUtils.h"
#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"
#include <memory>

namespace MetaInit 
{
	extern VkAllocationCallbacks* g_host_alloc;

	/*vulkan info inital functions*/
	class VulkanQueue
	{

	};

	class VulkanDevice
	{
	public:
		using Ptr = std::shared_ptr<VulkanDevice>;
		static Ptr Create() { return Ptr(new VulkanDevice); }
		VkDevice Get() { return device_; }
		VkPhysicalDevice operator[](int index);
	private:
		
	private:
		VkDevice						device_ = VK_NULL_HANDLE;
		Vector<VkPhysicalDevice>		phy_devices_;
	};
	
	class VulkanInstance
	{
	public:
		struct VulkanInstanceParams
		{

		};
		using Parameters = VulkanInstanceParams;
		using Ptr = std::unique_ptr<VulkanInstance>;
		static Ptr Create(const Parameters& params);
		VkInstance Get() { return instance_; }
		~VulkanInstance() { vkDestroyInstance(instance_, g_host_alloc); }
	private:
		VkInstance instance_ = VK_NULL_HANDLE;
	};
	
}

