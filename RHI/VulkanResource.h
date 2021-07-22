#pragma once
#include "VulkanRHI.h"
#include "VulkanResource.h"
#include <unordered_map>
#include <list>
#include <mutex>

namespace MetaInit
{
	/*functions to make create informations for resource related structure*/
	VkImageCreateInfo MakeImageCreateInfo(VkImageCreateFlags flags, VkFormat format);
	VkImageViewCreateInfo MakeImageViewCreateInfo(VkImageViewCreateFlags flags, VkImage image, const VkImageCreateInfo& imageInfo);
	VkBufferCreateInfo MakeBufferCreateInfo(VkBufferCreateFlags flags, uint32_t size, 
											const SmallVector<uint32_t>& family_indices);
	VkBufferViewCreateInfo MakeBufferViewCreateInfo(VkBufferViewCreateFlags flags, VkBuffer buffer,
													VkFormat format, VkDeviceSize offset, VkDeviceSize range);
	VkSamplerCreateInfo MakeSamplerCreateInfo(VkSamplerCreateFlags flags, VkFilter mag_filter, VkFilter min_filter, VkSamplerMipmapMode mipmap_mode,
											VkSamplerAddressMode address_modeu, VkSamplerAddressMode address_modev, VkSamplerAddressMode address_modew);
	VkDescriptorSetAllocateInfo MakeDescriptorSetAllocateInfo(VkDescriptorPool pool, VkDescriptorSetLayout layout);
	VkDescriptorSetLayoutCreateInfo MakeDescriptorSetLayoutCreateInfo(VkDescriptorSetLayoutCreateFlags flags,
																		const SmallVector<VkDescriptorSetLayoutBinding>& bindings);
	VkDescriptorPoolCreateInfo MakeDescriptorPoolCreateInfo(VkDescriptorPoolCreateFlags flags, uint32_t max_sets, 
															const SmallVector<VkDescriptorPoolSize>& pool_size);


	class DescriptorPool
	{
	public:
		using Ptr = std::shared_ptr<DescriptorPool>;
		DescriptorPool() = default;
		explicit DescriptorPool(VulkanDevice::Ptr device, uint32_t set_size,
								const VkDescriptorSetLayoutCreateInfo& layout_info);
		DISALLOW_COPY_AND_ASSIGN(DescriptorPool);
		void Init(VulkanDevice::Ptr device, uint32_t set_size,
					const VkDescriptorSetLayoutCreateInfo& layout);
		void UnInit();
		VkDescriptorSet CreateDescriptorSet();
		Vector<VkDescriptorSet> CreateDescriptorSets(uint32_t batch_size);
		void Reset();
		~DescriptorPool() { UnInit(); }
	private:
		void UpdateCurrPool();
	private:
		friend class DescriptorPoolManager;
		using List = std::list<VkDescriptorPool>;
		List available_;
		List used_;
		VkDescriptorPool			curr_{VK_NULL_HANDLE};
		VulkanDevice::Ptr			device_;
		VkDescriptorSetLayout		layout_;
		uint32_t					pool_size_;
		std::mutex					pool_mutex_;
	};

	class DescriptorPoolManager
	{
	public:
		using Ptr = std::shared_ptr<DescriptorPoolManager>;
		DescriptorPoolManager() = default;
		DISALLOW_COPY_AND_ASSIGN(DescriptorPoolManager);
		DescriptorPool::Ptr GetPool(VkDescriptorSetLayout layout);
	private:
		std::unordered_map<VkDescriptorSetLayout, DescriptorPool::Ptr> pools_;
	};

	class VulkanCmdBuffer;
	class DescriptorSetsWrapper
	{
	public:
		struct PseudoDescriptor
		{
			PseudoDescriptor(DescriptorSetsWrapper* wrapper, const std::string& desc_name) :wrapper_(wrapper) {}
			template <typename ResourceHandle>
			void operator=(ResourceHandle resource)
			{
				throw std::runtime_error("not implement a general func");
			}
			DescriptorSetsWrapper* wrapper_;
			std::string desc_name;
		};
		PseudoDescriptor operator[](std::string& desc_name);
		void Init();
		template <typename ...Args>
		void Write(int index, Args ...args);
		void Reset();
		void UnInit();
		VkDescriptorSet Get()const { return handle_; }
		DescriptorSetsWrapper() = default;
		DescriptorSetsWrapper(const DescriptorSetsWrapper&) = delete;
		DescriptorSetsWrapper& operator=(const DescriptorSetsWrapper&) = delete;
		~DescriptorSetsWrapper() = default;
	private:
		//configure descriptor 
		void UpdateImage(const Primitive::VulkanImage& image);
		void UpdateSampler(const Primitive::VulkanSampler& sampler);
		void UpdateBuffer(const Primitive::VulkanBuffer& buffer);
	private:
		VulkanDevice::Ptr				device_;
		DescriptorPoolManager			pool_repo_;
		VkDescriptorSet					handle_{ VK_NULL_HANDLE };
		VkDescriptorSetLayout			layouts_{ VK_NULL_HANDLE };
		using DescLut = std::unordered_map<std::string, uint32_t>;
		DescLut							desc_lut_;
	};
	
	template<>
	void DescriptorSetsWrapper::PseudoDescriptor::operator=(Primitive::VulkanImage& image)
	{
		wrapper_->UpdateImage();
	}

	template<>
	void DescriptorSetsWrapper::PseudoDescriptor::operator=(Primitive::VulkanSampler& sampler)
	{
		wrapper_->UpdateSampler();
	}

	template<>
	void DescriptorSetsWrapper::PseudoDescriptor::operator=(Primitive::VulkanBuffer& buffer)
	{
		wrapper_->UpdateBuffer();
	}
}
