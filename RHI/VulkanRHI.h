#pragma once

#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"
#include <memory>


namespace MetaInit 
{
	class VulkanDevice
	{
	public:
		VkDevice get() { return device_; }
	private:
		VkDevice device_;
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
		VkInstance get() { return instance_; }
		~VulkanInstance() { vkDestroyInstance(instance_, nullptr); }
	private:
		VkInstance instance_ = VK_NULL_HANDLE;
	};
	
}

