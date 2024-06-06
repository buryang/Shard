#include "Render/RenderResources.h"
#include "HAL/Vulkan/HALResourcesVulkan.h"
#include "HAL/Vulkan/HALResourceBindingVulkan.h"

namespace Shard::HAL::Vulkan {

    static inline EBindLessTag ConvertBufferDescToBindTag(const HALBufferInitializer& buffer_desc) {
        if (Utils::LogicAndFlags(buffer_desc.access_, EAccessFlags::eReadOnly) == buffer_desc.access_) {
            return eBufferSRVTag;
        }
        return eBufferUAVTag;
    }
    
    static inline EBindLessTag ConvertTextureDescToBindTag(const HALTextureInitializer& texture_desc) {
        if (Utils::LogicAndFlags(texture_desc.access_, EAccessFlags::eUAV) == texture_desc.access_) {
            return eTextureUAVTag;
        }
        else if (Utils::HasAnyFlags(texture_desc.access_, EAccessFlags::eSRV)) { //fixme
            return eTextureSRVSamplerTag;
        }
        return eTextureSRVTag;
    }

    HALResourceBindlessSetVulkan::~HALResourceBindlessSetVulkan()
    {
    }
    void HALResourceBindlessSetVulkan::Init(const HALBindLessTableInitializer& initializer)
    {
        descriptor_heaps_.clear();
        VulkanPipelineLayoutDesc pipe_desc;
        for (auto n = 0; n < initializer.num_member_; ++n) {
            const auto& desc = initializer.members_[n];
            if (desc.tag_ < eNum) {
                CreateDescriptorHeap(desc, pipe_desc);
                tag_set_index_.emplace(eastl::make_pair(desc.tag_, n));
            }
            else if (desc.tag_ == EBindLessRangeTag::ePushRangeTag) {
                //deal with push constant 
                PCHECK(desc.extra_info_ != nullptr) << "push range is null";
                auto push_range = *reinterpret_cast<HALBindLessPushRange*>(desc.extra_info_);
                pipe_desc.AddConstRange(VkPushConstantRange{ VK_SHADER_STAGE_ALL, push_range.offset_, push_range.size_ });
            }
        }

        vkCreatePipelineLayout(parent_->Get(), &pipe_desc.ToVulkan(), g_host_alloc_vulkan, &pipeline_layout_);
    }
    
    void HALResourceBindlessSetVulkan::UnInit()
    {
        vkDestroyPipelineLayout(parent_->Get(), pipeline_layout_, g_host_alloc_vulkan);
    }

    void HALResourceBindlessSetVulkan::Bind(HALCommandContext* command)
    {
        /*
        const auto cmd_flags = command->GetFlags();
        VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
        if (cmd_flags == HALCommandContext::EFlags::eAsyncCompute) {
            bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
        }
        */
    }
    HALResourceHandle HALResourceBindlessSetVulkan::WriteBuffer(HALBuffer* buffer)
    {
        auto buffer_vulkan = dynamic_cast<HALBufferVulkan*>(buffer);
        const auto tag = ConvertBufferDescToBindTag(buffer->GetBufferDesc());
        const auto buffer_handle = GetAvailableResourceHandle(tag);
        const auto set_index = GetDescriptorHeapIndex(tag);
        auto& set = *descriptor_heaps_[set_index].set_;
        set.BeginUpdates();
        set[uint32_t(buffer_handle)] = (VkBuffer)buffer_vulkan->GetHandle();
        set.EndUpdates();
        return buffer_handle;
    }
    HALResourceHandle HALResourceBindlessSetVulkan::WriteTexture(HALTexture* texture)
    {
        auto texture_vulkan = dynamic_cast<HALTextureVulkan*>(texture);
        const auto tag = ConvertTextureDescToBindTag(texture->GetTextureDesc());
        const auto texture_handle = GetAvailableResourceHandle(tag);
        const auto set_index = GetDescriptorHeapIndex(tag);
        auto& set = *descriptor_heaps_[set_index].set_;
        set.BeginUpdates();
        set[uint32_t(texture_handle)] = *texture_vulkan->GetImpl();
        set.EndUpdates();
        return texture_handle;
    }
    void HALResourceBindlessSetVulkan::Notify(const HALNotifyHeader& header, const Span<uint8_t>& notify_data)
    {
        PCHECK(IsBind()) << "resource bindless is binded";
        HALPushConstant cmd_packet;
        cmd_packet.flags_ = header.flags_;
        cmd_packet.user_data_ = notify_data;
        cmd_packet.offset_ = header.offset_;
        cmd_buffer_->Enqueue(&cmd_packet);
    }
    uint32_t HALResourceBindlessSetVulkan::GetDescriptorHeapIndex(uint32_t tag_flags)
    {
        return tag_set_index_[tag_flags];
    }
    
    void HALResourceBindlessSetVulkan::CreateDescriptorHeap(const HALBindLessTableInitializer::Member& desc, VulkanPipelineLayoutDesc& pipe_desc)
    {
        //create one pool for each descriptor set
        VulkanPoolCreateDesc pool_desc;
        pool_desc.flags_ = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        VkDescriptorType desc_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        uint32_t max_descriptor_count = 0;
        const auto device_index_props = GetGlobalDevice()->GetPhysicalDeviceDescriptorIndexingProperties();
        switch (desc.tag_) {
        case eBufferSRVTag:
            desc_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            max_descriptor_count = device_index_props.maxDescriptorSetUpdateAfterBindUniformBuffers;
            break;
        case eBufferUAVTag:
            desc_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            max_descriptor_count = device_index_props.maxDescriptorSetUpdateAfterBindStorageBuffers;
            break;
        case eTextureSRVTag:
            desc_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            max_descriptor_count = device_index_props.maxDescriptorSetUpdateAfterBindSampledImages;
            break;
        case eTextureSRVSamplerTag:
            desc_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            max_descriptor_count = device_index_props.maxDescriptorSetUpdateAfterBindSampledImages;
            break;
        case eTextureUAVTag:
            desc_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            max_descriptor_count = device_index_props.maxDescriptorSetUpdateAfterBindStorageImages;
            break;
        default:
            PLOG(ERROR) << "not supported bindless descriptor type";
        };
        pool_desc.AddPoolSize({desc_type, max_descriptor_count});
        auto pool_ptr = VulkanDescriptorPoolManager::Instance().GetPool(pool_desc);

        //create descriptor set 
        VkDescriptorSetLayoutBinding layout_bindings[2];
        layout_bindings[0].stageFlags = VK_SHADER_STAGE_ALL;
        layout_bindings[0].binding = 0;
        layout_bindings[0].pImmutableSamplers = VK_NULL_HANDLE; 
        layout_bindings[0].descriptorType = desc_type;
        layout_bindings[0].descriptorCount = eastl::min(max_descriptor_count, desc.max_size_);

        VulkanDescriptorSetLayoutDesc set_layout_desc;
        //https://github.com/KhronosGroup/Vulkan-Docs/issues/985
        if (desc.tag_ == eTextureSRVSamplerTag && desc.extra_info_ != nullptr) {
            const auto& sampler_info = *reinterpret_cast<VulkanImmutableSamplers*>(desc.extra_info_);
            //Set texture binding start at the end of the immutable samplers
            layout_bindings[0].binding = sampler_info.count_;

            layout_bindings[1].binding = 0;
            layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            layout_bindings[1].stageFlags = VK_SHADER_STAGE_ALL;
            layout_bindings[1].descriptorCount = sampler_info.count_;
            layout_bindings[1].pImmutableSamplers = sampler_info.samplers_;
            set_layout_desc.AddBinding("sampler", layout_bindings[1], 0x0);
        }
        set_layout_desc.AddBinding("bindless", layout_bindings[0], VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);
        VulkanDescriptorSet::SharedPtr set_ptr(new VulkanDescriptorSet(pool_ptr, set_layout_desc));
        descriptor_heaps_.emplace_back(HeapData{ pool_ptr, set_ptr });

        //add set layout to pipeline layout
        pipe_desc.AddSet(set_layout_desc, set_ptr->GetLayout());
    }
}
