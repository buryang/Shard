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
		switch (image_type)
		{
		case	 VK_IMAGE_TYPE_1D:
			return VK_IMAGE_VIEW_TYPE_1D;
		case VK_IMAGE_TYPE_2D:
			return VK_IMAGE_VIEW_TYPE_2D;
		default:
			assert(0&&"image type not supported");
		}
	}

	VkImageViewCreateInfo MakeImageViewCreateInfo(VkImage image, const VkImageCreateInfo& image_info)
	{
		VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		view_info.pNext = VK_NULL_HANDLE;
		view_info.format = image_info.format;
		view_info.viewType = GetImageViewType(image_info.imageType);
		return view_info;
	}


	DescriptorPool::DescriptorPool(VulkanDevice device, uint32_t set_size,
										const VkDescriptorSetLayoutCreateInfo& layout)
	{
		Init(device, set_size, layout);
	}

	void DescriptorPool::Init(VulkanDevice device, uint32_t set_size,
		const VkDescriptorSetLayoutCreateInfo& layout_info)
	{
		device_ = device;
		curr_ = VK_NULL_HANDLE;

		//todo check device support layout
		VkDescriptorSetLayoutSupport support;
		vkGetDescriptorSetLayoutSupport(device_.Get(), &layout_info, &support);

		if (!support.supported)
		{
			throw std::runtime_error("descriptorset layout not supported by device");
		}

		Span<const VkDescriptorSetLayoutBinding> bindings{ layout_info.pBindings, layout_info.bindingCount };

		uint32_t desc_buffer_count = 0, desc_tex_count = 0;
		for (auto& binding : bindings)
		{
			if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
				binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
			{
				++desc_buffer_count;
			}
			else if (0)
			{
				++desc_tex_count;
			}
		}
		
		pool_size_ = (desc_buffer_count + desc_tex_count) * set_size;
		auto ret = vkCreateDescriptorSetLayout(device_.Get(), &layout_info, g_host_alloc, &layout_);
		assert(ret == VK_SUCCESS && "create descriptor layout failed");
	}

	void DescriptorPool::UnInit()
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
		vkDestroyDescriptorSetLayout(device_.Get(), layout_, g_host_alloc);
	}

	VkDescriptorSet DescriptorPool::CreateDescriptorSet()
	{

	}

	void DescriptorPool::Reset()
	{
		std::copy(used_.begin(), used_.end(), avalaible_.end());
		used_.clear();

		for (auto pool : avalaible_)
		{
			vkResetDescriptorPool(device_.Get(), pool, 0);
		}

		curr_ = VK_NULL_HANDLE;
	}


	void DescriptorPool::AddNewPool()
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