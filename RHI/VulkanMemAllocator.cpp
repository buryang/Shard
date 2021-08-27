#include "RHI/VulkanMemAllocator.h"

namespace MetaInit {

	static constexpr inline uint32_t GetVulkanApiVersion()
	{
#if VMA_VULKAN_VERSION == 1002000
		return VK_API_VERSION_1_2;
#elif VMA_VULKAN_VERSION == 1001000
		return VK_API_VERSION_1_1;
#elif VMA_VULKAN_VERSION == 1000000
		return VK_API_VERSION_1_0;
#else
#error Invalid VMA_VULKAN_VERSION.
		return UINT32_MAX;
#endif
	}

	VmaAllocatorCreateInfo MakeVmaAllocatorCreateInfo(VmaAllocatorCreateFlags flags)
	{
		VmaAllocatorCreateInfo alloc_info{};
		memset(&alloc_info, 0, sizeof(VmaAllocatorCreateInfo));
		alloc_info.flags = flags;
		alloc_info.vulkanApiVersion = GetVulkanApiVersion();
		return alloc_info;
	}

	VmaAllocationCreateInfo MakeVmaAllocationCreateInfo(VmaAllocationCreateFlags flags)
	{
		VmaAllocationCreateInfo alloc_info{};
		memset(&alloc_info, 0, sizeof(VmaAllocationCreateInfo));
		alloc_info.flags = flags;
		alloc_info.pUserData = VK_NULL_HANDLE;
		return alloc_info;
	}

	VulkanVMAWrapper::VulkanVMAWrapper(VulkanDevice::Ptr device, VulkanInstance::Ptr instance):device_(device)
	{
		auto alloc_info = MakeVmaAllocatorCreateInfo(VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT);
		alloc_info.device = device->Get();
		alloc_info.instance = instance->Get();
		alloc_info.physicalDevice = device->GetPhy();
		alloc_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		alloc_info.vulkanApiVersion = VK_API_VERSION_1_2;
		vmaCreateAllocator(&alloc_info, &handle_);
	}
	
	VMAAllocation VulkanVMAWrapper::CreateImage(size_t size, const VkImageCreateInfo& info, VkImageLayout layout, VkMemoryPropertyFlags props)
	{
		VMAAllocation result;

		auto vma_info = MakeVmaAllocationCreateInfo();
		vma_info.usage = VMA_MEMORY_USAGE_UNKNOWN;
		//vma_info.flags = info.flags; //FIXME

		auto ret = vmaCreateImage(handle_, &vma_info, nullptr, &result.image_, &result.allocation_, nullptr);
		assert(ret == VK_SUCCESS);
		return result;
	}

	VMAAllocation VulkanVMAWrapper::CreateBuffer(VkDeviceSize size, const VkBufferCreateInfo& info, VkMemoryPropertyFlags props)
	{
		VMAAllocation result;
		
		auto vma_info = MakeVmaAllocationCreateInfo();
		vma_info.requiredFlags = 0;

		auto ret = vmaCreateBuffer(handle_, &info, &vma_info, &result.buffer_, &result.allocation_, nullptr);
		assert(ret == VK_SUCCESS);
		return result;
	}

	/*use dedicated allocator?*/
	VMAAllocation VulkanVMAWrapper::CreateAccleration(VkAccelerationStructureCreateInfoKHR& info, VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
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