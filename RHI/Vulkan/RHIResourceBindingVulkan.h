#pragma once
#include "RHI/Vulkan/API/VulkanDescriptor.h"
#include "RHI/RHIResourceBinding.h"

namespace MetaInit::RHI::Vulkan {

	struct VulkanImmutableSamplers
	{
		uint32_t	count_{ 0u };
		VkSampler*	samplers_{ nullptr };
	};

	class RHIResourceBindlessSetVulkan : public RHIResourceBindlessHeap
	{
	public:
		using Ptr = RHIResourceBindlessSetVulkan*;
		using SharedPtr = eastl::shared_ptr<RHIResourceBindlessSetVulkan>;
		RHIResourceBindlessSetVulkan() = default;
		~RHIResourceBindlessSetVulkan();
		void Init(const RHIBindLessTableInitializer& desc)override;
		void Bind(RHICommandContext::Ptr command)override;
		RHIResourceHandle WriteBuffer(RHIBuffer::Ptr buffer)override;
		RHIResourceHandle WriteTexture(RHITexture::Ptr texture)override;
		void Notify(const RHINotifyHeader& header, const Span<uint8_t>& notify_data)override;
		FORCE_INLINE VkPipelineLayout GetPipelineLayout()const {
			return pipeline_layout_;
		}
		FORCE_INLINE uint32_t GetDescriptorSetCount()const {
			return descriptor_heaps_.size();
		}
		FORCE_INLINE VulkanDescriptorSet::SharedPtr GetDescritorSet(uint32_t index)const {
			return descriptor_heaps_[index].set_;
		}
	private:
		uint32_t GetDescriptorHeapIndex(uint32_t tag_flags);
		void CreateDescriptorHeap(const RHIBindLessTableInitializer::Member& desc, VulkanPipelineLayoutDesc& pipe_desc);
	private:
		Map<uint32_t, uint32_t>	tag_set_index_;
		struct HeapData {
			VulkanDescriptorPool::SharedPtr pool_;
			VulkanDescriptorSet::SharedPtr set_;
		};
		SmallVector<HeapData> descriptor_heaps_;
		VkPipelineLayout	pipeline_layout_;
	};
}
