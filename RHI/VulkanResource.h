#pragma once
#include "VulkanRHI.h"
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
		void Init();
		void Bind(VulkanCmdBuffer& cmd);
		void Update();
		template <typename ...Args>
		void Write(int index, Args ...args);
		void Reset();
		void UnInit();
		DescriptorSetsWrapper() = default;
		DescriptorSetsWrapper(const DescriptorSetsWrapper&) = delete;
		DescriptorSetsWrapper& operator=(const DescriptorSetsWrapper&) = delete;
		~DescriptorSetsWrapper();
	private:
		VulkanDevice::Ptr				device_;
		DescriptorPoolManager			pool_repo_;
		Vector<VkDescriptorSet>			sets_;
		Vector<VkDescriptorSetLayout>	layouts_;
	};
}
