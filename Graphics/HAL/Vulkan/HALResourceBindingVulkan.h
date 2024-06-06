#pragma once
#include "HAL/Vulkan/API/VulkanDescriptor.h"
#include "HAL/HALResourceBinding.h"

namespace Shard::HAL::Vulkan {

    struct VulkanImmutableSamplers
    {
        uint32_t    count_{ 0u };
        VkSampler*    samplers_{ nullptr };
    };

    class HALResourceBindlessSetVulkan : public HALResourceBindlessHeap, public VulkanDeviceObject
    {
    public:
        using Ptr = HALResourceBindlessSetVulkan*;
        using SharedPtr = eastl::shared_ptr<HALResourceBindlessSetVulkan>;
        HALResourceBindlessSetVulkan() = default;
        ~HALResourceBindlessSetVulkan();
        void Init(const HALBindLessTableInitializer& desc)override;
        void UnInit()override;
        void Bind(HALCommandContext* command)override;
        HALResourceHandle WriteBuffer(HALBuffer* buffer)override;
        HALResourceHandle WriteTexture(HALTexture* texture)override;
        void Notify(const HALNotifyHeader& header, const Span<uint8_t>& notify_data)override;
        FORCE_INLINE VkPipelineTiling GetPipelineTiling()const {
            return pipeline_layout_;
        }
        FORCE_INLINE uint32_t GetDescriptorSetCount()const {
            return descriptor_heaps_.size();
        }
        FORCE_INLINE VulkanDescriptorSet::SharedPtr GetDescritorSet(uint32_t index)const {
            return descriptor_heaps_[index].set_;
        }
        ~HALResourceBindlessSetVulkan() {
            UnInit();
        }
    private:
        uint32_t GetDescriptorHeapIndex(uint32_t tag_flags);
        void CreateDescriptorHeap(const HALBindLessTableInitializer::Member& desc, VulkanPipelineTilingDesc& pipe_desc);
    private:
        Map<uint32_t, uint32_t>    tag_set_index_;
        struct HeapData {
            VulkanDescriptorPool::SharedPtr pool_;
            VulkanDescriptorSet::SharedPtr set_;
        };
        SmallVector<HeapData> descriptor_heaps_;
        VkPipelineTiling    pipeline_layout_;
    };
}
