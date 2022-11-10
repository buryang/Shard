#pragma once
#include "RHI/Vulkan/API/VulkanRHI.h"
#include "RHI/Vulkan/API/VulkanResource.h"
#include <mutex>

//https://stackoverflow.com/questions/48867995/how-to-correctly-use-decriptor-sets-for-multiple-interleaving-buffers
namespace MetaInit
{
	/*functions to make create informations for resource related structure*/
	VkImageCreateInfo MakeImageCreateInfo(VkImageCreateFlags flags, VkFormat format);
	VkImageViewCreateInfo MakeImageViewCreateInfo(VkImageViewCreateFlags flags, VkImage image, const VkImageCreateInfo& imageInfo);
	VkBufferCreateInfo MakeBufferCreateInfo(VkBufferCreateFlags flags, uint32_t size);
	VkBufferViewCreateInfo MakeBufferViewCreateInfo(VkBufferViewCreateFlags flags, VkBuffer buffer,
													VkFormat format, VkDeviceSize offset, VkDeviceSize range);
	VkSamplerCreateInfo MakeSamplerCreateInfo(VkSamplerCreateFlags flags, VkFilter mag_filter, VkFilter min_filter, VkSamplerMipmapMode mipmap_mode,
											VkSamplerAddressMode address_modeu, VkSamplerAddressMode address_modev, VkSamplerAddressMode address_modew);
	VkDescriptorSetAllocateInfo MakeDescriptorSetAllocateInfo(VkDescriptorPool pool, const VkDescriptorSetLayout* layout, const uint32_t layout_count);
	VkDescriptorSetLayoutCreateInfo MakeDescriptorSetLayoutCreateInfo(VkDescriptorSetLayoutCreateFlags flags,
																		const Span<VkDescriptorSetLayoutBinding>& bindings);
	VkDescriptorPoolCreateInfo MakeDescriptorPoolCreateInfo(VkDescriptorPoolCreateFlags flags, uint32_t max_sets, 
															const Span<VkDescriptorPoolSize>& pool_size);
	VkPipelineLayoutCreateInfo MakePipelineLayoutCreateInfo();


	struct VulkanPoolDesc
	{
		uint32_t	set_size_;
		struct DescCount
		{
			uint32_t	desc_type_;
			uint32_t	desc_count_;
		};
		//Vector<DescCount> desc_count_;
	};

	class VulkanDescriptorPool
	{
	public:
		using Desc = VulkanPoolDesc;
		using SharedPtr = std::shared_ptr<VulkanDescriptorPool>;
		using Handle = VkDescriptorPool;
		VulkanDescriptorPool() = default;
		explicit VulkanDescriptorPool(const Desc& desc);
		void Init(const Desc& desc);
		void UnInit();
		VkDescriptorSet CreateDescriptorSet(VkDescriptorSetLayout layout);
		SmallVector<VkDescriptorSet> CreateDescriptorSets(const SmallVector<VkDescriptorSetLayout>& layouts);
		void Reset();
		~VulkanDescriptorPool() { UnInit(); }
	private:
		DISALLOW_COPY_AND_ASSIGN(VulkanDescriptorPool);
		bool CanAllocate(VkDescriptorSetLayout layout)const;
		void UpdateCurrPool();
	private:
		friend class DescriptorPoolManager;
		using List = List<VkDescriptorPool>;
		List available_;
		List used_;
		Handle	handle_{ VK_NULL_HANDLE };
		uint32_t	alloc_size_ = 0;//size allocated from curr pool
		uint32_t	pool_size_;
		std::mutex	pool_mutex_;
	};

	class VulkanDescriptorPoolManager
	{
	public:
		using SharedPtr = std::shared_ptr<VulkanDescriptorPoolManager>;
		static SharedPtr Instance();
		VulkanDescriptorPool::SharedPtr GetPool(uint32_t key);
	private:
		VulkanDescriptorPoolManager() = default;
		DISALLOW_COPY_AND_ASSIGN(VulkanDescriptorPoolManager);
	private:
		Map<uint32_t, VulkanDescriptorPool::SharedPtr>	pools_;
	};

	struct VulkanPushConstantRangeDesc
	{
		VkShaderStageFlags	flags_{ VK_SHADER_STAGE_ALL };
		uint32_t	offset_{ 0 };
		uint32_t	size_{ 0 };
		FORCE_INLINE bool IsValid()const {
			return Utils::AlignDown(offset_ , 4) == offset_ && Utils::AlignDown(size_, 4) == size_;
		}
	};

	struct VulkanDescriptorSetDesc
	{
		struct VulkanDescriptorBindDesc
		{
			uint32_t	binding_{ 0 };
			uint32_t	desc_type_{ VK_DESCRIPTOR_TYPE_SAMPLER };
			//desc count > 0 for desccriptor indexing
			uint32_t	desc_count_{ 1 };
			VkShaderStageFlags	flags_{ VK_SHADER_STAGE_ALL };
			String	binding_name_;
			//the sampler objects must not be destroyed before the final 
			//use of the set layout and any descriptor pools and sets created using it
			VkSampler	sampler{ VK_NULL_HANDLE };
			bool IsSamplerMutable()const {
				return !((desc_type_ == VK_DESCRIPTOR_TYPE_SAMPLER || desc_type_ == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
					&& sampler != VK_NULL_HANDLE);
			}
		};
		SmallVector<VulkanDescriptorBindDesc>	bindings_;
		String	name_;
		FORCE_INLINE uint32_t GetBindingSize() const { return bindings_.size(); }
	};

	//like dxr RootSignature
	struct VulkanPipelineLayoutDesc
	{
		void AddSet(VulkanDescriptorSetDesc& desc_set);
		void AddConstRange(VulkanPushConstantRangeDesc& const_range);
		FORCE_INLINE uint32_t GetNumConstants()const { return consts_.size(); }
		FORCE_INLINE uint32_t GetNumDescriptorSets()const { return desc_sets_.size(); }
		FORCE_INLINE const VulkanPushConstantRangeDesc& GetConstantDesc(uint32_t index)const {
			return push_consts_[index];
		}
		FORCE_INLINE const VulkanDescriptorSetDesc& GetDescriptorSetDesc(uint32_t index)const {
			return desc_sets_[index];
		}
		SmallVector<VulkanPushConstantRangeDesc>	push_consts_;
		SmallVector<VulkanDescriptorSetDesc>	desc_sets_;
	};

	class VulkanCmdBuffer;
	class VulkanAttachment;
	class VulkanDescriptorSet : public std::enable_shared_from_this<VulkanDescriptorSet>
	{
	public:
		using Handle = VkDescriptorSet;
		using SharedPtr = eastl::shared_ptr<VulkanDescriptorSet>;
		struct PseudoDescriptor
		{
			PseudoDescriptor(SharedPtr wrapper, uint32_t binding) : wrapper_(wrapper), desc_binding_(binding) {}
			void operator=(const VulkanImage& image);
			void operator=(VkSampler sampler);
			void operator=(const VulkanBuffer& buffer);
			void operator=(const VulkanBufferView& buffer_view);
			void operator=(const VulkanAttachment& attach);
			SharedPtr wrapper_;
			uint32_t desc_binding_{ 0 };
		};
		PseudoDescriptor operator[](const String& desc_name);
		void Init(const VulkanPipelineLayoutDesc& desc_set, VkDescriptorSet handle);
		void BeginUpdates();
		void EndUpdates();
		void UnInit();
		FORCE_INLINE const String& GetName()const { return name_; }
		FORCE_INLINE Handle Get()const { return handle_; }
		VulkanDescriptorSet() = default;
		DISALLOW_COPY_AND_ASSIGN(VulkanDescriptorSet);
		//descriptor pool to manage resource release
		~VulkanDescriptorSet();
	private:
		//configure descriptor 
		void UpdateImage(const VulkanImage& image, uint32_t binding);
		void UpdateSampler(VkSampler sampler, uint32_t binding);
		void UpdateBuffer(const VulkanBuffer& buffer, uint32_t binding, uint32_t offset);
		void UpdateBufferView(const VulkanBufferView& buffer_view, uint32_t binding);
		void UpdateInAttachment(const VulkanAttachment& attach, uint32_t binding);
		void UpdateAccelerationStructure(void*, uint32_t binding);
	private:
		Handle handle_{ VK_NULL_HANDLE };
		Map<String, uint32_t>	descLUT_;
		SmallVector<VkWriteDescriptorSet>	write_cache_;
		bool	read_only_{ false };
		String	name_;
		//descriptor set write data container
		struct VulkanDescriptorInfoDetail
		{
			union
			{
				VkDescriptorBufferInfo	buffer_info_;
				VkDescriptorImageInfo	image_info_;
				VkBufferView buffer_view_;
			};
		};
		SmallVector<VulkanDescriptorInfoDetail>	desc_infos_;
	};




}
