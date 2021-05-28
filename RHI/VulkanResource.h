#pragma once
#include "VulkanRHI.h"

namespace MetaInit
{
	VkImageCreateInfo MakeImageCreateInfo(VkImageCreateFlags flags, VkFormat format);
	VkImageViewCreateInfo MakeImageViewCreateInfo(VkImage image, const VkImageCreateInfo& imageInfo);
	VkBufferCreateInfo MakeBufferCreateInfo();
	VkBufferViewCreateInfo MakeBufferViewCreateInfo();
	VkSamplerCreateInfo MakeSamplerCreateInfo();
	VkDescriptorSetLayoutCreateInfo MakeDescriptorSetLayoutCreateInfo();
	VkDescriptorPoolCreateInfo MakeDescriptorPoolCreateInfo();

	class DescriptorPool
	{

	};

	class DescriptorPoolManager
	{
	public:
		DescriptorPoolManager() = default;
		DescriptorPoolManager(VulkanDevice device, uint32_t set_size,
								const VkDescriptorSetLayoutCreateInfo& layout);
		DISALLOW_COPY_AND_ASSIGN(DescriptorPoolManager);
		void Init(VulkanDevice device, uint32_t set_size,
					const VkDescriptorSetLayoutCreateInfo& layout);
		void UnInit();
		VkDescriptorSet CreateDescriptorSet();
		void Reset();
		~DescriptorPoolManager() { UnInit(); }
	private:
		void AddNewPool();
	private:
		using List = std::list<VkDescriptorPool>;
		List avalaible_;
		List used_;
		VkDescriptorPool curr_{VK_NULL_HANDLE};
		VulkanDevice device_;
		VkDescriptorSetLayout layout_;
		size_t pool_size_;
	};

	class DescriptorSetsWrapper
	{
	public:
		void Init();
		void Bind(VulkanCmdBuffer cmd);
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
		VulkanDevice device_;
		DescriptorPoolManager pool_repo_;
		Vector<VkDescriptorSet> sets_;
		Vector<VkDescriptorSetLayout> layouts_;
	};
}
