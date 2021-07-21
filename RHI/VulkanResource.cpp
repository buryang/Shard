#include "VulkanResource.h"
#include "VulkanCmdContext.h"

namespace MetaInit
{
	VkImageCreateInfo MakeImageCreateInfo(VkImageCreateFlags flags, VkFormat format)
	{
		VkImageCreateInfo image_info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		image_info.pNext = VK_NULL_HANDLE;
		image_info.flags = flags;
		image_info.format = format;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		//not support cubemap now
		image_info.arrayLayers = 1;
		image_info.mipLevels = 1;
		return image_info;
	}

	static inline VkImageViewType GetImageViewType(VkImageType image_type)
	{
		switch (image_type)
		{
		case VK_IMAGE_TYPE_1D:
			return VK_IMAGE_VIEW_TYPE_1D;
		case VK_IMAGE_TYPE_2D:
			return VK_IMAGE_VIEW_TYPE_2D;
		default:
			throw std::invalid_argument("not supported image view type");
		}
	}

	VkImageViewCreateInfo MakeImageViewCreateInfo(VkImageViewCreateFlags flags, VkImage image, const VkImageCreateInfo& imageInfo)
	{
		VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		view_info.flags = flags;
		view_info.pNext = VK_NULL_HANDLE;
		view_info.viewType = GetImageViewType(image_info.imageType);
		return view_info;
	}

	VkBufferCreateInfo MakeBufferCreateInfo(VkBufferCreateFlags flags, uint32_t size,
		const SmallVector<uint32_t>& family_indices)
	{
		VkBufferCreateInfo buffer_info{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		buffer_info.flags = flags;
		buffer_info.pNext = VK_NULL_HANDLE;
		buffer_info.size = size;
		if ( !family_indices.empty())
		{
			buffer_info.queueFamilyIndexCount = family_indices.size();
			buffer_info.pQueueFamilyIndices = family_indices.data();
		}
		else
		{
			buffer_info.queueFamilyIndexCount = 0;
			buffer_info.pQueueFamilyIndices = VK_NULL_HANDLE;
		}
		return buffer_info;
	}

	VkBufferViewCreateInfo MakeBufferViewCreateInfo(VkBufferViewCreateFlags flags, VkBuffer buffer,
		VkFormat format, VkDeviceSize offset, VkDeviceSize range)
	{
		VkBufferViewCreateInfo view_info{ VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
		view_info.flags = flags;
		view_info.pNext = VK_NULL_HANDLE;
		view_info.buffer = buffer;
		view_info.format = format;
		view_info.offset = offset;
		view_info.range = range;
		return view_info;
	}

	VkSamplerCreateInfo MakeSamplerCreateInfo(VkSamplerCreateFlags flags, VkFilter mag_filter, VkFilter min_filter, 
												VkSamplerMipmapMode mipmap_mode, VkSamplerAddressMode address_modeu, 
												VkSamplerAddressMode address_modev, VkSamplerAddressMode address_modew)
	{
		VkSamplerCreateInfo sample_info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		sample_info.flags = flags;
		sample_info.magFilter = mag_filter;
		sample_info.minFilter = min_filter;
		sample_info.addressModeU = address_modeu;
		sample_info.addressModeV = address_modev;
		sample_info.addressModeW = address_modew;
		return sample_info;
	}

	VkDescriptorSetAllocateInfo MakeDescriptorSetAllocateInfo(VkDescriptorPool pool, VkDescriptorSetLayout layout)
	{
		VkDescriptorSetAllocateInfo desc_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		desc_info.pNext = VK_NULL_HANDLE;
		desc_info.descriptorPool = pool;
		desc_info.descriptorSetCount = 1;
		desc_info.pSetLayouts = &layout;
		return desc_info;
	}

	VkDescriptorSetLayoutCreateInfo MakeDescriptorSetLayoutCreateInfo(VkDescriptorSetLayoutCreateFlags flags,
																	const SmallVector<VkDescriptorSetLayoutBinding>& bindings)
	{
		VkDescriptorSetLayoutCreateInfo desc_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		desc_info.flags = flags;
		desc_info.pNext = VK_NULL_HANDLE;
		if (!bindings.empty())
		{
			desc_info.bindingCount = bindings.size();
			desc_info.pBindings = bindings.data();
		}
		else
		{
			desc_info.bindingCount = 0;
			desc_info.pBindings = VK_NULL_HANDLE;
		}
		return desc_info;
	}

	VkDescriptorPoolCreateInfo MakeDescriptorPoolCreateInfo(VkDescriptorPoolCreateFlags flags, uint32_t max_sets,
		const SmallVector<VkDescriptorPoolSize>& pool_size)
	{
		VkDescriptorPoolCreateInfo pool_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		pool_info.flags = flags;
		pool_info.pNext = VK_NULL_HANDLE;
		pool_info.maxSets = max_sets;
		if (!pool_size.empty())
		{
			pool_info.poolSizeCount = pool_size.size();
			pool_info.pPoolSizes = pool_size.data();
		}
		else
		{
			pool_info.poolSizeCount = 0;
			pool_info.pPoolSizes = VK_NULL_HANDLE;
		}
		return pool_info;
	}

	DescriptorPool::DescriptorPool(VulkanDevice::Ptr device, uint32_t set_size,
										const VkDescriptorSetLayoutCreateInfo& layout)
	{
		Init(device, set_size, layout);
	}

	void DescriptorPool::Init(VulkanDevice::Ptr device, uint32_t set_size,
		const VkDescriptorSetLayoutCreateInfo& layout_info)
	{
		device_ = device;
		curr_ = VK_NULL_HANDLE;

		//todo check device support layout
		VkDescriptorSetLayoutSupport support;
		vkGetDescriptorSetLayoutSupport(device_->Get(), &layout_info, &support);

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
		auto ret = vkCreateDescriptorSetLayout(device_->Get(), &layout_info, g_host_alloc, &layout_);
		assert(ret == VK_SUCCESS && "create descriptor layout failed");
	}

	void DescriptorPool::UnInit()
	{
		for (auto pool : available_)
		{
			vkDestroyDescriptorPool(device_->Get(), pool, g_host_alloc);
		}
		available_.clear();

		for (auto pool : used_)
		{
			vkDestroyDescriptorPool(device_->Get(), pool, g_host_alloc);
		}
		used_.clear();
		vkDestroyDescriptorSetLayout(device_->Get(), layout_, g_host_alloc);
	}

	VkDescriptorSet DescriptorPool::CreateDescriptorSet()
	{
		auto desc_creater = [&](VkDescriptorPool pool) {
			VkDescriptorSet desc_set = VK_NULL_HANDLE;
			VkDescriptorSetAllocateInfo desc_info = MakeDescriptorSetAllocateInfo(pool, layout_);
			auto ret = vkAllocateDescriptorSets(device_->Get(), &desc_info, &desc_set);
			if (ret == VK_SUCCESS)
			{
				return desc_set;
			}
			throw std::runtime_error("alloc desc set failed");
		};
		
		std::lock_guard<std::mutex> lock_pool(pool_mutex_);

		if (curr_ == VK_NULL_HANDLE)
		{
			UpdateCurrPool();
		}

		try
		{
			auto desc_set = desc_creater(curr_);
			return desc_set;
		}
		catch (std::runtime_error& e)
		{
			UpdateCurrPool();
			auto desc_set = desc_creater(curr_);
			return desc_set;
		}

	}

	Vector<VkDescriptorSet> DescriptorPool::CreateDescriptorSets(uint32_t batch_size)
	{
		return Vector<VkDescriptorSet>();
	}

	void DescriptorPool::Reset()
	{
		for (auto pool : used_)
		{
			//flags is reversed for future usage
			vkResetDescriptorPool(device_->Get(), pool, 0);
		}
		available_.insert(available_.end(), used_.begin(), used_.end());
		used_.clear();
		curr_ = VK_NULL_HANDLE;
	}


	void DescriptorPool::UpdateCurrPool()
	{
		if (available_.empty())
		{
			VkDescriptorPoolCreateInfo pool_info = MakeDescriptorPoolCreateInfo(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, pool_size_, );
			VkDescriptorPool pool;
			auto ret = vkCreateDescriptorPool(device_->Get(), &pool_info, g_host_alloc, &pool);
			assert(ret != VK_SUCCESS);
			available_.push_back(pool);
		}
		curr_ = available_.front();
		available_.pop_front();
		used_.push_back(curr_);
	}

	DescriptorPool::Ptr DescriptorPoolManager::GetPool(VkDescriptorSetLayout layout)
	{
		if (pools_.find(layout) == pools_.end())
		{
			DescriptorPool::Ptr pool_ptr(new DescriptorPool);
			pools_.insert(std::make_pair(layout, pool));
		}
		return pools_[layout];
	}

}