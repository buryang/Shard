#pragma once
#include "RHI/VulkanRHI.h"
#include "RHI/VulkanPrimitive.h"
#include <unordered_map>
#include <list>
#include <mutex>

//https://stackoverflow.com/questions/48867995/how-to-correctly-use-decriptor-sets-for-multiple-interleaving-buffers
namespace MetaInit
{
	/*functions to make create informations for resource related structure*/
	VkImageCreateInfo MakeImageCreateInfo(VkImageCreateFlags flags, VkFormat format);
	VkImageViewCreateInfo MakeImageViewCreateInfo(VkImageViewCreateFlags flags, VkImage image, const VkImageCreateInfo& imageInfo);
	VkBufferCreateInfo MakeBufferCreateInfo(VkBufferCreateFlags flags, uint32_t size, 
											const SmallVector<uint32_t>& family_indices=SmallVector<uint32_t>());
	VkBufferViewCreateInfo MakeBufferViewCreateInfo(VkBufferViewCreateFlags flags, VkBuffer buffer,
													VkFormat format, VkDeviceSize offset, VkDeviceSize range);
	VkSamplerCreateInfo MakeSamplerCreateInfo(VkSamplerCreateFlags flags, VkFilter mag_filter, VkFilter min_filter, VkSamplerMipmapMode mipmap_mode,
											VkSamplerAddressMode address_modeu, VkSamplerAddressMode address_modev, VkSamplerAddressMode address_modew);
	VkDescriptorSetAllocateInfo MakeDescriptorSetAllocateInfo(VkDescriptorPool pool, const VkDescriptorSetLayout* layout, const uint32_t layout_count);
	VkDescriptorSetLayoutCreateInfo MakeDescriptorSetLayoutCreateInfo(VkDescriptorSetLayoutCreateFlags flags,
																		const SmallVector<VkDescriptorSetLayoutBinding>& bindings);
	VkDescriptorPoolCreateInfo MakeDescriptorPoolCreateInfo(VkDescriptorPoolCreateFlags flags, uint32_t max_sets, 
															const SmallVector<VkDescriptorPoolSize>& pool_size);


	class DescriptorPool
	{
	public:
		typedef struct _PoolDesc
		{
			uint32_t	set_size_;
			struct DescCount
			{
				uint32_t	desc_type_;
				uint32_t	desc_count_;
			};
			//Vector<DescCount> desc_count_;
		}Desc;
		using Ptr = std::shared_ptr<DescriptorPool>;
		DescriptorPool() = default;
		explicit DescriptorPool(VulkanDevice::Ptr device, const Desc& desc);
		DISALLOW_COPY_AND_ASSIGN(DescriptorPool);
		void Init(VulkanDevice::Ptr device, const Desc& desc);
		void UnInit();
		VkDescriptorSet CreateDescriptorSet(VkDescriptorSetLayout layout);
		Vector<VkDescriptorSet> CreateDescriptorSets(const Vector<VkDescriptorSetLayout>& layouts);
		void Reset();
		~DescriptorPool() { UnInit(); }
	private:
		bool CanAllocate(VkDescriptorSetLayout layout)const;
		void UpdateCurrPool();
	private:
		friend class DescriptorPoolManager;
		using List = std::list<VkDescriptorPool>;
		List available_;
		List used_;
		VkDescriptorPool			curr_{ VK_NULL_HANDLE };
		VulkanDevice::Ptr			device_;
		uint32_t					alloc_size_ = 0;//size allocated from curr pool
		uint32_t					pool_size_;
		std::mutex					pool_mutex_;
	};

	class DescriptorPoolManager
	{
	public:
		using Ptr = std::shared_ptr<DescriptorPoolManager>;
		DescriptorPoolManager() = default;
		DISALLOW_COPY_AND_ASSIGN(DescriptorPoolManager);
		DescriptorPool::Ptr GetPool(uint32_t hash);
	private:
		Map<uint32_t, DescriptorPool::Ptr>	pools_;
	};

	//name from dxr
	class RootSignature
	{
	public:
		struct ConstantRangeDesc
		{
			uint32_t		flags_;
			uint32_t		offset_;
			uint32_t		size_;
			bool isValid()const;
		};

		struct DescriptorSetDesc
		{
			struct DescriptorBindDesc
			{
				uint32_t			binding_		= 0;
				uint32_t			desc_type_		= VK_DESCRIPTOR_TYPE_SAMPLER;
				//desc count > 0 for desccriptor indexing
				uint32_t			desc_count_		= 1;
				uint32_t			stage_flags_	= VK_SHADER_STAGE_ALL;
				std::string			binding_name_;
				//the sampler objects must not be destroyed before the final 
				//use of the set layout and any descriptor poolsand sets created using it
				VkSampler			sampler = VK_NULL_HANDLE;
				bool IsSamplerMutable()const {
					return !((desc_type_ == VK_DESCRIPTOR_TYPE_SAMPLER || desc_type_ == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
								&& sampler != VK_NULL_HANDLE);
				}
			};
			Vector<DescriptorBindDesc>	bindings_;
			std::string					set_name_;
			uint32_t Size()const { return bindings_.size(); }
		};
		void AddSet(DescriptorSetDesc& desc_set);
		void AddConstRange(ConstantRangeDesc& const_range);
		uint32_t GetNumConstants()const { return consts_.size(); }
		uint32_t GetNumDescriptorSets()const { return desc_sets_.size(); }
		const ConstantRangeDesc& GetConstantDesc(uint32_t index)const {
			return consts_[index];
		}
		const DescriptorSetDesc& GetDescriptorSetDesc(uint32_t index)const {
			return desc_sets_[index];
		}
	private:
		Vector<ConstantRangeDesc>		consts_;
		Vector<DescriptorSetDesc>		desc_sets_;
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
		void Init(const RootSignature::DescriptorSetDesc& desc_set, VkDescriptorSet handle);
		void BeginUpdates();
		void EndUpdates();
		void UnInit();
		const std::string& Name()const { return name_; }
		VkDescriptorSet Get()const { return handle_; }
		DescriptorSetsWrapper() = default;
		DISALLOW_COPY_AND_ASSIGN(DescriptorSetsWrapper);
		//descriptor pool to manage resource release
		~DescriptorSetsWrapper() = default;
	private:
		//configure descriptor 
		void UpdateImage(const Primitive::VulkanImage& image, uint32_t binding);
		void UpdateSampler(const Primitive::VulkanSampler& sampler, uint32_t binding);
		void UpdateBuffer(const Primitive::VulkanBuffer& buffer, uint32_t binding, uint32_t offset);
		void UpdateBufferView(const Primitive::VulkanBufferView& buffer_view, uint32_t binding);
		void UpdateInAttachment(const VulkanAttachment& attach, uint32_t binding);
		void UpdateAccelerationStructure(void*, uint32_t binding);
	private:
		VulkanDevice::Ptr				device_;
		VkDescriptorSet					handle_{ VK_NULL_HANDLE };
		using DescLut = Map<std::string, uint32_t>;
		DescLut							desc_lut_;
		Vector<VkWriteDescriptorSet>	write_cache_;
		bool							read_only_ = false;
		std::string						name_;
		//descriptor set write data container
		struct DescriptorInfoDetail
		{
			union
			{
				VkDescriptorBufferInfo	buffer_info_;
				VkDescriptorImageInfo	image_info_;
				//WriteDescriptorSetAccelerationStructureKHR acc_info_;
			};
		};
		Vector<DescriptorInfoDetail>	desc_infos_;
	};




}
