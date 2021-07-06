#include "VulkanMemAllocator.h"

namespace MetaInit {

	VulkanVMAWrapper::VulkanVMAWrapper(VulkanDevice::Ptr device, VulkanInstance& instance):device_(device)
	{
		VmaAllocatorCreateInfo alloc_info;
		alloc_info.device = device->Get();
		alloc_info.instance = instance.Get();
		alloc_info.physicalDevice = device->GetPhy();
		alloc_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		alloc_info.vulkanApiVersion = VK_API_VERSION_1_2;
		vmaCreateAllocator(&alloc_info, &alloc_);
	}
	
	VMAAllocation VulkanVMAWrapper::CreateImage(size_t size, const VkImageCreateInfo& info,
										VkImageLayout layout, VkMemoryPropertyFlags props)
	{
		VMAAllocation result;

		VmaAllocationCreateInfo vma_info;
		vma_info.usage = VMA_MEMORY_USAGE_UNKNOWN;
		vma_info.flags = info.flags; //FIXME

		auto ret = vmaCreateImage(alloc_, &info, &vma_info, &result.image_, &result.allocation_, nullptr);
		assert(ret == VK_SUCCESS);
		return result;
	}

	VMAAllocation VulkanVMAWrapper::CreateBuffer(VkDeviceSize size, const VkBufferCreateInfo& info, VkMemoryPropertyFlags props)
	{
		VMAAllocation result;
		
		VmaAllocationCreateInfo vma_info;
		vma_info.requiredFlags = 0;

		auto ret = vmaCreateBuffer(alloc_, &info, &vma_info, &result.buffer_, &result.allocation_, nullptr);
		assert(ret == VK_SUCCESS);
		return result;
	}

	/*use dedicated allocator?*/
	VMAAccerleraion VulkanVMAWrapper::CreateAccleration(VkAccelerationStructureCreateInfoKHR& info, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	{
		VMAAccerleraion result;
		VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		buffer_info.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		buffer_info.size = info.size;

		result.alloc_ = CreateBuffer(info.size, buffer_info, props);
		info.buffer = result.alloc_.buffer_;
		vkCreateAccelerationStructureKHR(device_->Get(), &info, g_host_alloc, &result.acc_);
		
		return result;
	}

	void VulkanVMAWrapper::DestroyImage(VMAAllocation& image)
	{
		vkDestroyImage(device_->Get(), image.image_, g_host_alloc);
		vmaFreeMemory(alloc_, image.allocation_);
	}

	void VulkanVMAWrapper::DestroyBuffer(VMAAllocation& buffer)
	{
		vkDestroyBuffer(device_->Get(), buffer.buffer_, g_host_alloc);
		vmaFreeMemory(alloc_, buffer.allocation_);
	}

	void VulkanVMAWrapper::DestroyAccleration(VMAAccerleraion& acc)
	{
		vkDestroyAccelerationStructureKHR(device_->Get(), acc.acc_, g_host_alloc);
		DestroyBuffer(acc.alloc_);
	}

	VulkanVMAWrapper::~VulkanVMAWrapper()
	{
		if (VK_NULL_HANDLE != alloc_)
		{
			vmaDestroyAllocator(alloc_);
		}
	}
}