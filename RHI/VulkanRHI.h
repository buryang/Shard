#pragma once
#include "Utils/CommonUtils.h"
#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"
#include <memory>

namespace MetaInit 
{
	extern VkAllocationCallbacks* g_host_alloc;

	VkDeviceCreateInfo MakeDeviceCreateInfo(VkDeviceCreateFlags flags, const Vector<VkDeviceQueueCreateInfo>& queue_infos,
											const Vector<const char*>& ext_infos, const Vector<const char*>& layer_infos,
											const Vector<VkPhysicalDeviceFeatures>& device_features);
	VkInstanceCreateInfo MakeInstanceCreateInfo(VkInstanceCreateFlags flags, const VkApplicationInfo& app_info,
											const Vector<const char*> ext_infos, const Vector<const char*>& layer_infos);

	class VulkanDevice
	{
	public:
		using Ptr = std::shared_ptr<VulkanDevice>;
		static Ptr Create(const VkDeviceCreateInfo& device_info);
		VkDevice Get() { return device_; }
		VkQueue GetQueue(uint32_t family_index, uint32_t index)const;
		VkPhysicalDevice operator[](uint32_t index);
	private:
		VulkanDevice() = default;
		DISALLOW_COPY_AND_ASSIGN(VulkanDevice);
	private:
		VkDevice									device_ = VK_NULL_HANDLE;
		Vector<VkPhysicalDevice>					phy_devices_;
		VkDeviceCreateInfo						device_info_;
		//std::unordered_map<uint32_t, uint32_t>  queue_info_;
	};
	
	class VulkanInstance
	{
	public:
		using Ptr = std::unique_ptr<VulkanInstance>;
		static Ptr Create(const VkInstanceCreateInfo& params);
		VkInstance Get() { return instance_; }
		~VulkanInstance() { vkDestroyInstance(instance_, g_host_alloc); }
	private:
		VkInstance						instance_ = VK_NULL_HANDLE;
		Vector<VkExtensionProperties>	extensions_;
		VulkanDevice						device_;
	};
	
}

