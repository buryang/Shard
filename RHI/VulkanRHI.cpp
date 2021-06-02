#include "VulkanRHI.h"
#include "GLM/glm.hpp"

#include <string>
#include <vector>
#include <fstream>

namespace MetaInit
{
	/*for:"this feature is not used, the implementation will 
	  perform its own memory allocations. Since most memory 
	  allocations are off the critical path", so just set null*/
	VkAllocationCallbacks* g_host_alloc = VK_NULL_HANDLE;

	VulkanDevice::Ptr VulkanDevice::Create(const VkDeviceCreateInfo& device_info)
	{
		VulkanDevice::Ptr device_ptr{ new VulkanDevice};
		auto ret = vkCreateDevice(nullptr, &device_info, g_host_alloc, &device_ptr->device_);
		assert(ret == VK_SUCCESS && "create device failed");
		device_ptr->device_info_ = device_info;
		return device_ptr;
	}

	VkQueue VulkanDevice::GetQueue(uint32_t family_index, uint32_t index)const
	{
		VkQueue queue;
		vkGetDeviceQueue(device_, family_index, index, &queue);
		return queue;
	}
	
	
	VkPhysicalDevice VulkanDevice::operator[](uint32_t index)
	{
		assert(index < phy_devices_.size());
		return phy_devices_[index];
	}




}