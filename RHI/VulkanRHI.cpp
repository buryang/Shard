#include "RHI/VulkanRHI.h"
#include "RHI/VulkanMemAllocator.h"
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

	VkQueue VulkanDevice::GetQueue(VkQueueFlags flags)const
	{
		uint32_t family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(phy_devices_, &family_count, nullptr);
		SmallVector<VkQueueFamilyProperties> family_props(family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(phy_devices_, &family_count, family_props.data());

		for (auto findex = 0; findex < family_props.size(); ++findex)
		{
			auto& prop = family_props[findex];
			if (prop.queueFlags & flags)
			{
				//to check flags
				return GetQueue(findex, 0);
			}
		}

		return VK_NULL_HANDLE;
	}

	bool VulkanDevice::IsFormatSupported(VkFormat format)const
	{
		VkImageFormatProperties format_prop;
		/*If format is not a supported image format, or if the combination of format, type, tiling, usage, and
			flags is not supported for images, then vkGetPhysicalDeviceImageFormatProperties returns VK_ERROR_FORMAT_NOT_SUPPORTED.*/
		auto ret = vkGetPhysicalDeviceImageFormatProperties(phy_devices_, format, VK_IMAGE_TYPE_2D,
															VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, 0, &format_prop);
		return ret != VK_ERROR_FORMAT_NOT_SUPPORTED;
	}
	
	/*
	VkPhysicalDevice VulkanDevice::operator[](uint32_t index)
	{
		assert(index < phy_devices_.size());
		return phy_devices_[index];
	}
	*/
	void VulkanDevice::WaitIdle()const
	{
		vkDeviceWaitIdle(device_);
	}



}