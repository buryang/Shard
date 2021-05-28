#include "VulkanResource.h"
#include "VulkanCmdContext.h"

#include <span>


namespace MetaInit
{
	VkImageCreateInfo MakeImageCreateInfo(VkImageCreateFlags flags, VkFormat format)
	{
		VkImageCreateInfo image_info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		image_info.pNext = VK_NULL_HANDLE;
		image_info.flags = flags;
		image_info.format = format;
		return image_info;
	}

	static inline VkImageViewType GetImageViewType(VkImageType image_type)
	{

	}

	VkImageViewCreateInfo MakeImageViewCreateInfo(VkImage image, const VkImageCreateInfo& image_info)
	{
		VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		view_info.pNext = VK_NULL_HANDLE;
		view_info.format = image_info.format;
		view_info.viewType = GetImageViewType(image_info.imageType);
		return view_info;
	}


	DescriptorPoolManager::DescriptorPoolManager(VulkanDevice device, size_t pool_size,
													const VkDescriptorSetLayoutCreateInfo& layout)
	{
		Init(device, pool_size, layout);
	}

	void DescriptorPoolManager::Init(VulkanDevice device, size_t pool_size,
										const VkDescriptorSetLayoutCreateInfo& layout)
	{
		device_ = device;
		curr_ = VK_NULL_HANDLE;

		//todo check device support layout
		VkDescriptorSetLayoutSupport support;
		vkGetDescriptorSetLayoutSupport(device_.Get(), &layout, &support);

		if (!support.supported)
		{
			throw std::runtime_error("descriptorset layout not supported by device");
		}

		std::span<>
	}

	void DescriptorPoolManager::UnInit()
	{
		for (auto pool : avalaible_)
		{
			vkDestroyDescriptorPool(device_.Get(), pool, g_host_alloc);
		}
		avalaible_.clear();

		for (auto pool : used_)
		{
			vkDestroyDescriptorPool(device_.Get(), pool, g_host_alloc);
		}
		used_.clear();
	}

	VkDescriptorSet DescriptorPoolManager::CreateDescriptorSet()
	{

	}

	void DescriptorPoolManager::Reset()
	{
		std::copy(used_.begin(), used_.end(), avalaible_.end());
		used_.clear();

		for (auto pool : avalaible_)
		{
			vkResetDescriptorPool(device_.Get(), pool, 0);
		}

		curr_ = VK_NULL_HANDLE;
	}


	void DescriptorPoolManager::AddNewPool()
	{
		VkDescriptorPoolCreateInfo pool_info = MakeDescriptorPoolCreateInfo();
		//TODO
		VkDescriptorPool pool;
		auto ret = vkCreateDescriptorPool(device_.Get(), &pool_info, g_host_alloc, &pool);
		assert(VK_NULL_HANDLE == ret);
		used_.emplace_back(pool);
		curr_ = pool;
	}


}