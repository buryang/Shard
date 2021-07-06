#pragma once

#include "VulkanRHI.h"
#include "vma/vk_mem_alloc.h"

namespace MetaInit {

	struct VMAAllocation
	{
		union {
			VkImage		image_{ VK_NULL_HANDLE };
			VkBuffer	buffer_;
		};
		VmaAllocation	allocation_{ nullptr };
	};

	struct VMAAccerleraion
	{
		VMAAllocation				alloc_;
		VkAccelerationStructureKHR	acc_{VK_NULL_HANDLE};
	};

	class VulkanVMAWrapper {
	public:
		VulkanVMAWrapper(VulkanDevice::Ptr device, VulkanInstance& instance);
		VMAAllocation CreateImage(size_t size,
							const VkImageCreateInfo& info,
							VkImageLayout layout,
							VkMemoryPropertyFlags props);
		VMAAllocation CreateBuffer(VkDeviceSize size,
							const VkBufferCreateInfo& info,
							VkMemoryPropertyFlags props);
		VMAAccerleraion CreateAccleration(VkAccelerationStructureCreateInfoKHR& info,
								VkMemoryPropertyFlags props);
		void DestroyImage(VMAAllocation& image);
		void DestroyBuffer(VMAAllocation& buffer);
		void DestroyAccleration(VMAAccerleraion& acc);
		VulkanVMAWrapper(const VulkanVMAWrapper&) = delete;
		VulkanVMAWrapper& operator=(const VulkanVMAWrapper&) = delete;
		~VulkanVMAWrapper();
	private:
		VulkanVMAWrapper() = default;
	private:
		VulkanDevice::Ptr	device_;
		VmaAllocator		alloc_{VK_NULL_HANDLE};
	};
}
