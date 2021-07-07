#pragma once
#include "VulkanRHI.h"
#include <unordered_map>
#include <list>

namespace MetaInit
{
	/*functions to make create informations for resource related structure*/
	VkImageCreateInfo MakeImageCreateInfo(VkImageCreateFlags flags, VkFormat format);
	VkImageViewCreateInfo MakeImageViewCreateInfo(VkImage image, const VkImageCreateInfo& imageInfo);
	VkBufferCreateInfo MakeBufferCreateInfo(VkBufferCreateFlags flags, uint32_t size, 
											const SmallVector<uint32_t>& family_indices);
	VkBufferViewCreateInfo MakeBufferViewCreateInfo(VkBufferViewCreateFlags flags, VkBuffer buffer,
													VkFormat format, VkDeviceSize offset, VkDeviceSize range);
	VkSamplerCreateInfo MakeSamplerCreateInfo(VkSamplerCreateFlags flags);
	VkDescriptorSetLayoutCreateInfo MakeDescriptorSetLayoutCreateInfo(VkDescriptorSetLayoutCreateFlags flags,
																		const SmallVector<VkDescriptorSetLayoutBinding>& bindings);
	VkDescriptorPoolCreateInfo MakeDescriptorPoolCreateInfo(VkDescriptorPoolCreateFlags flags, uint32_t max_sets, 
															const SmallVector<VkDescriptorPoolSize>& pool_size);


	class DescriptorPool
	{
	public:
		DescriptorPool() = default;
		DescriptorPool(VulkanDevice::Ptr device, uint32_t set_size,
								const VkDescriptorSetLayoutCreateInfo& layout_info);
		DISALLOW_COPY_AND_ASSIGN(DescriptorPool);
		void Init(VulkanDevice::Ptr device, uint32_t set_size,
					const VkDescriptorSetLayoutCreateInfo& layout);
		void UnInit();
		VkDescriptorSet CreateDescriptorSet();
		void Reset();
		~DescriptorPool() { UnInit(); }
	private:
		void AddNewPool();
	private:
		friend class DescriptorPoolManager;
		using List = std::list<VkDescriptorPool>;
		List available_;
		List used_;
		VkDescriptorPool			curr_{VK_NULL_HANDLE};
		VulkanDevice::Ptr			device_;
		VkDescriptorSetLayout		layout_;
		uint32_t					pool_size_;

	};

	class DescriptorPoolManager
	{
	public:
		DescriptorPoolManager() = default;
		DISALLOW_COPY_AND_ASSIGN(DescriptorPoolManager);
		void AddPool(VkDescriptorSetLayout layout, DescriptorPool&& pool);
		DescriptorPool& GetPool(VkDescriptorSetLayout layout);
		DescriptorPool& operator[](VkDescriptorSetLayout layout);
	private:
		std::unordered_map<VkDescriptorSetLayout, DescriptorPool> pools_;
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
		VulkanDevice					device_;
		DescriptorPoolManager			pool_repo_;
		Vector<VkDescriptorSet>			sets_;
		Vector<VkDescriptorSetLayout>	layouts_;
	};
}
