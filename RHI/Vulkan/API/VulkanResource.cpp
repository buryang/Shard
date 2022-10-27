#include "RHI/VulkanResource.h"
#include "RHI/VulkanCmdContext.h"
#include "RHI/VulkanRenderPass.h"

namespace MetaInit
{
	VkImageCreateInfo MakeImageCreateInfo(VkImageCreateFlags flags, VkFormat format)
	{
		VkImageCreateInfo image_info{};
		memset(&image_info, 0, sizeof(image_info));
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
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

	static inline VkImageViewCreateInfo MakeImageViewCreateInfo(VkImageViewCreateFlags flags, VkImage image, VkImageViewType view_type)
	{
		VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		view_info.flags = flags;
		view_info.pNext = VK_NULL_HANDLE;
		view_info.viewType = view_type;
		return view_info;
	}

	static inline VkBufferCreateInfo MakeBufferCreateInfo(VkBufferCreateFlags flags, uint32_t size,
															const SmallVector<uint32_t>& family_indices)
	{
		VkBufferCreateInfo buffer_info{};
		memset(&buffer_info, 0, sizeof(buffer_info));
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.flags = flags;
		buffer_info.pNext = VK_NULL_HANDLE;
		buffer_info.size = size;
		if ( !family_indices.empty())
		{
			buffer_info.queueFamilyIndexCount = family_indices.size();
			buffer_info.pQueueFamilyIndices = family_indices.data();
		}
		return buffer_info;
	}

	VkBufferViewCreateInfo MakeBufferViewCreateInfo(VkBufferViewCreateFlags flags, VkBuffer buffer,
		VkFormat format, VkDeviceSize offset, VkDeviceSize range)
	{
		VkBufferViewCreateInfo view_info{};
		memset(&view_info, 0, sizeof(view_info));
		view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
		view_info.flags = flags;
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
		VkSamplerCreateInfo sample_info{};
		memset(&sample_info, 0, sizeof(sample_info));
		sample_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sample_info.flags = flags;
		sample_info.magFilter = mag_filter;
		sample_info.minFilter = min_filter;
		sample_info.addressModeU = address_modeu;
		sample_info.addressModeV = address_modev;
		sample_info.addressModeW = address_modew;
		return sample_info;
	}

	VkDescriptorSetAllocateInfo MakeDescriptorSetAllocateInfo(VkDescriptorPool pool, const VkDescriptorSetLayout* layout, const uint32_t layout_count)
	{
		VkDescriptorSetAllocateInfo desc_info{};
		memset(&desc_info, 0, sizeof(desc_info));
		desc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		desc_info.descriptorPool = pool;
		desc_info.descriptorSetCount = layout_count;
		desc_info.pSetLayouts = layout;
		return desc_info;
	}

	VkDescriptorSetLayoutCreateInfo MakeDescriptorSetLayoutCreateInfo(VkDescriptorSetLayoutCreateFlags flags,
																			const SmallVector<VkDescriptorSetLayoutBinding>& bindings)
	{
		VkDescriptorSetLayoutCreateInfo desc_info{};
		memset(&desc_info, 0, sizeof(desc_info));
		desc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		desc_info.flags = flags;
		if (!bindings.empty())
		{
			desc_info.bindingCount = bindings.size();
			desc_info.pBindings = bindings.data();
		}
		return desc_info;
	}

	VkDescriptorPoolCreateInfo MakeDescriptorPoolCreateInfo(VkDescriptorPoolCreateFlags flags, uint32_t max_sets,
																	const SmallVector<VkDescriptorPoolSize>& pool_size)
	{
		VkDescriptorPoolCreateInfo pool_info{};
		memset(&pool_info, 0, sizeof(pool_info));
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = flags;
		pool_info.pNext = VK_NULL_HANDLE;
		pool_info.maxSets = max_sets;
		if (!pool_size.empty())
		{
			pool_info.poolSizeCount = pool_size.size();
			pool_info.pPoolSizes = pool_size.data();
		}
		return pool_info;
	}

	DescriptorPool::DescriptorPool(VulkanDevice::Ptr device, const DescriptorPool::Desc& desc)
	{
		Init(device, desc);
	}

	void DescriptorPool::Init(VulkanDevice::Ptr device, const DescriptorPool::Desc& desc)
	{
		device_ = device;
		UpdateCurrPool();
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
	}

	VkDescriptorSet DescriptorPool::CreateDescriptorSet(VkDescriptorSetLayout layout)
	{
		auto desc_creater = [&](VkDescriptorPool pool) {
			VkDescriptorSet desc_set = VK_NULL_HANDLE;
			VkDescriptorSetAllocateInfo desc_info = MakeDescriptorSetAllocateInfo(pool, &layout, 1);
			auto ret = vkAllocateDescriptorSets(device_->Get(), &desc_info, &desc_set);
			if (ret == VK_SUCCESS)
			{
				return desc_set;
			}
			throw std::runtime_error("alloc desc set failed");
		};
		
		std::lock_guard<eastl::mutex> lock_pool(pool_mutex_);

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

	Vector<VkDescriptorSet> DescriptorPool::CreateDescriptorSets(const Vector<VkDescriptorSetLayout>& layouts)
	{
		Vector<VkDescriptorSet> desc_sets(layouts.size());
		auto desc_creater = [&](VkDescriptorPool pool) {
			VkDescriptorSetAllocateInfo desc_info = MakeDescriptorSetAllocateInfo(pool, layouts.data(), layouts.size());
			auto ret = vkAllocateDescriptorSets(device_->Get(), &desc_info, desc_sets.data());
			if (ret == VK_SUCCESS)
			{
				return;
			}
			throw std::runtime_error("alloc desc set failed");
		};

		std::lock_guard<eastl::mutex> lock_pool(pool_mutex_);
		if (curr_ == VK_NULL_HANDLE)
		{
			UpdateCurrPool();
		}

		try
		{
			if (!CanAllocate())
			{
				UpdateCurrPool();
			}
			desc_creater(curr_);
			return desc_sets;
		}
		catch (std::runtime_error& e)
		{
			throw std::runtime_error("descriptor set alloc failed");
		}

	}

	bool DescriptorPool::CanAllocate(VkDescriptorSetLayout layout)const
	{
		//fixme memory logic
		return alloc_size_ < pool_size_;
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

	DescriptorPool::Ptr DescriptorPoolManager::GetPool(uint32_t hash)
	{
		if (pools_.find(hash) == pools_.end())
		{
			DescriptorPool::Ptr pool_ptr(new DescriptorPool);
			pools_.insert(std::make_pair(hash, pool_ptr));
		}
		return pools_[hash];
	}

	DescriptorSetsWrapper::PseudoDescriptor DescriptorSetsWrapper::operator[](std::string& desc_name)
	{
		return PseudoDescriptor(this, desc_lut_[desc_name]);
	}

	void DescriptorSetsWrapper::Init(const RootSignature::DescriptorSetDesc& desc_set, VkDescriptorSet handle)
	{
		name_ = desc_set.set_name_;
		auto& bind_cfgs = desc_set.bindings_;
		desc_lut_.clear();
		for (auto n = 0; n < bind_cfgs.size(); ++n)
		{
			desc_lut_[bind_cfgs[n].binding_name_] = n;
		}
		desc_infos_.resize(bind_cfgs.size());
		handle_ = handle;
	}

	void DescriptorSetsWrapper::BeginUpdates()
	{
		write_cache_.clear();
	}

	void DescriptorSetsWrapper::EndUpdates()
	{
		if (!write_cache_.empty())
		{
			vkUpdateDescriptorSets(device_->Get(), write_cache_.size(), write_cache_.data(), 0, VK_NULL_HANDLE);
		}
	}

	void DescriptorSetsWrapper::UnInit()
	{
		//do nothing descriptor pool resp for release descriptorset
	}

	static inline VkDescriptorType SetDescriptorType(bool read_only, bool is_buffer, bool is_texel)
	{
		if (is_buffer) {
			if (is_texel)
			{
				return read_only ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
			}
			else
			{
				return read_only ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			}
		}
		else
		{
			return read_only ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		}
	}

	void DescriptorSetsWrapper::UpdateImage(const Primitive::VulkanImage& image, uint32_t binding)
	{
		VkWriteDescriptorSet write_set{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write_set.pNext = VK_NULL_HANDLE;
		write_set.dstSet = handle_;
		write_set.dstBinding = binding;
		write_set.descriptorCount = 1;
		write_set.descriptorType = SetDescriptorType(read_only_, false, false);

		auto* image_info = new (&desc_infos_[binding])VkDescriptorImageInfo;
		image_info->imageLayout = read_only_ ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
		image_info->imageView = 0;
		image_info->sampler = nullptr;
		write_set.pImageInfo = image_info;
		write_cache_.emplace_back(write_cache_);
	}

	void DescriptorSetsWrapper::UpdateSampler(const Primitive::VulkanSampler& sampler, uint32_t binding)
	{
		VkWriteDescriptorSet write_set{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write_set.pNext = VK_NULL_HANDLE;
		write_set.dstSet = handle_;
		write_set.dstBinding = binding;
		write_set.descriptorCount = 1;
		write_set.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		auto* image_info = new(&desc_infos_[binding])VkDescriptorImageInfo;
		image_info->sampler = sampler.Get();
		write_set.pImageInfo = image_info;
		write_cache_.emplace_back(write_set);
	}

	void DescriptorSetsWrapper::UpdateBuffer(const Primitive::VulkanBuffer& buffer, uint32_t binding, uint32_t offset)
	{
		VkWriteDescriptorSet write_set{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write_set.pNext = VK_NULL_HANDLE;
		write_set.dstSet = handle_;
		write_set.dstBinding = binding;
		write_set.descriptorCount = 1;
		write_set.descriptorType = SetDescriptorType(read_only_, true, false);
		auto* buffer_info = new(&desc_infos_[binding])VkDescriptorBufferInfo;
		buffer_info->buffer = buffer.Get();
		buffer_info->offset = offset;
		buffer_info->range = buffer.GetSize();
		write_set.pBufferInfo = buffer_info;
		write_cache_.emplace_back(write_set);
		//todo
	}

	void DescriptorSetsWrapper::UpdateBufferView(const Primitive::VulkanBufferView& buffer_view, uint32_t binding)
	{
		VkWriteDescriptorSet write_set{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write_set.pNext = VK_NULL_HANDLE;
		write_set.dstSet = handle_;
		write_set.dstBinding = binding;
		write_set.descriptorCount = 1;
		write_set.descriptorType = SetDescriptorType(read_only_, true, true);
		write_set.pTexelBufferView = buffer_view.Get();
		write_cache_.emplace_back(write_set);
	}

	void DescriptorSetsWrapper::UpdateInAttachment(const VulkanAttachment& attach, uint32_t binding)
	{
		VkWriteDescriptorSet write_set{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write_set.pNext = VK_NULL_HANDLE;
		write_set.dstSet = handle_;
		write_set.dstBinding = binding;
		write_set.descriptorCount = 1;
		write_set.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		auto* image_info = new(&desc_infos_[binding])VkDescriptorImageInfo;
		image_info->sampler = VK_NULL_HANDLE;
		//image_info->imageView = attach.
		image_info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		write_set.pImageInfo = image_info; 
		write_cache_.emplace_back(write_set);
	}

	void DescriptorSetsWrapper::UpdateAccelerationStructure(void*, uint32_t binding)
	{
		/*If descriptorType is VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 
		 *the pNext chain must include a VkWriteDescriptorSetAccelerationStructureKHR
		 *structure whose accelerationStructureCount member equals descriptorCount */
		auto* acc_info = new(&desc_infos_[binding])VkWriteDescriptorSetAccelerationStructureKHR;
		memset(acc_info, 0, sizeof(*acc_info));
		acc_info->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		acc_info->accelerationStructureCount = 1;
		acc_info->pAccelerationStructures = nullptr; //todo
		VkWriteDescriptorSet write_set;
		memset(&write_set, 0, sizeof(write_set));
		write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_set.dstBinding = binding;
		write_set.dstSet = handle_;
		write_set.pNext = acc_info;
		write_set.descriptorCount = acc_info.accelerationStructureCount;
		write_cache_.emplace_back(write_set); 
	}

	void DescriptorSetsWrapper::PseudoDescriptor::operator=(const Primitive::VulkanImage& image)
	{
		wrapper_->UpdateImage(image, desc_binding_);
	}
	void DescriptorSetsWrapper::PseudoDescriptor::operator=(const Primitive::VulkanSampler& sampler)
	{
		wrapper_->UpdateSampler(sampler, desc_binding_);
	}
	void DescriptorSetsWrapper::PseudoDescriptor::operator=(const Primitive::VulkanBuffer& buffer)
	{
		wrapper_->UpdateBuffer(buffer, desc_binding_, 0);
	}
	void DescriptorSetsWrapper::PseudoDescriptor::operator=(const Primitive::VulkanBufferView& buffer_view)
	{
		wrapper_->UpdateBufferView(buffer_view, desc_binding_);
	}
	void DescriptorSetsWrapper::PseudoDescriptor::operator=(const VulkanAttachment& attach)
	{
		wrapper_->UpdateInAttachment(attach, desc_binding_);
	}

	void RootSignature::AddSet(DescriptorSetDesc& desc_set)
	{
		desc_sets_.emplace_back(desc_set);
	}

	void RootSignature::AddConstRange(ConstantRangeDesc& const_range)
	{
		consts_.emplace_back(const_range);
	}
	bool RootSignature::ConstantRangeDesc::isValid() const
	{
		return offset_ & ~0x3 == 0 && size_ & ~0x3 == 0 && size_ <= max_const_size;
	}
}