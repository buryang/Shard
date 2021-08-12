#pragma once
#include "RHI/VulkanRHI.h"
#include "RHI/VulkanPrimitive.h"
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
	class VulkanAttachment;
	class DescriptorSetsWrapper
	{
	public:
		struct PseudoDescriptor
		{
			PseudoDescriptor(DescriptorSetsWrapper* wrapper, uint32_t binding) :wrapper_(wrapper), desc_binding_(binding) {}
			void operator=(const Primitive::VulkanImage& image);
			void operator=(const Primitive::VulkanSampler& sampler);
			void operator=(const Primitive::VulkanBuffer& buffer);
			void operator=(const Primitive::VulkanBufferView& buffer_view);
			void operator=(const VulkanAttachment& attach);
			DescriptorSetsWrapper* wrapper_ = nullptr;
			uint32_t desc_binding_ = 0;
		};
		PseudoDescriptor operator[](std::string& desc_name);
		void Init();
		template <typename ...Args>
		void Write(int index, Args ...args);
		void BeginUpdates();
		void EndUpdates();
		void Reset();
		void UnInit();
		VkDescriptorSet Get()const { return handle_; }
		DescriptorSetsWrapper() = default;
		DescriptorSetsWrapper(const DescriptorSetsWrapper&) = delete;
		DescriptorSetsWrapper& operator=(const DescriptorSetsWrapper&) = delete;
		~DescriptorSetsWrapper() = default;
	private:
		//configure descriptor 
		void UpdateImage(const Primitive::VulkanImage& image, uint32_t binding);
		void UpdateSampler(const Primitive::VulkanSampler& sampler, uint32_t binding);
		void UpdateBuffer(const Primitive::VulkanBuffer& buffer, uint32_t binding, uint32_t offset);
		void UpdateBufferView(const Primitive::VulkanBufferView& buffer_view, uint32_t binding);
		void UpdateInAttachment(const VulkanAttachment& attach, uint32_t binding);
	private:
		VulkanDevice::Ptr				device_;
		DescriptorPoolManager			pool_repo_;
		VkDescriptorSet					handle_{ VK_NULL_HANDLE };
		VkDescriptorSetLayout			layouts_{ VK_NULL_HANDLE };
		using DescLut = std::unordered_map<std::string, uint32_t>;
		DescLut							desc_lut_;
		Vector<VkWriteDescriptorSet>	write_cache_;
		bool							read_only_ = false;
	};
	
}
