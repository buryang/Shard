#include "Renderer/RtRenderResources.h"
#include "RHI/Vulkan/RHIResourcesVulkan.h"
#include "RHI/Vulkan/RHIResourceBindingVulkan.h"

namespace Shard::RHI::Vulkan {

     static inline EBindLessTag ConvertBufferDescToBindTag(const RHIBufferInitializer& buffer_desc) {
          if (Utils::LogicAndFlags(buffer_desc.access_, Renderer::EAccessFlags::eReadOnly) == buffer_desc.access_) {
               return eBufferSRVTag;
          }
          return eBufferUAVTag;
     }
     
     static inline EBindLessTag ConvertTextureDescToBindTag(const RHITextureInitializer& texture_desc) {
          if (Utils::LogicAndFlags(texture_desc.access_, Renderer::EAccessFlags::eUAV) == texture_desc.access_) {
               return eTextureUAVTag;
          }
          else if (Utils::HasAnyFlags(texture_desc.access_, Renderer::EAccessFlags::eSRV)) { //fixme
               return eTextureSRVSamplerTag;
          }
          return eTextureSRVTag;
     }

     RHIResourceBindlessSetVulkan::~RHIResourceBindlessSetVulkan()
     {
     }
     void RHIResourceBindlessSetVulkan::Init(const RHIBindLessTableInitializer& initializer)
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
                    auto push_range = *reinterpret_cast<RHIBindLessPushRange*>(desc.extra_info_);
                    pipe_desc.AddConstRange(VkPushConstantRange{ VK_SHADER_STAGE_ALL, push_range.offset_, push_range.size_ });
               }
          }

          vkCreatePipelineLayout(GetGlobalDevice(), &pipe_desc.ToVulkan(), g_host_alloc, &pipeline_layout_);
     }
     
     void RHIResourceBindlessSetVulkan::UnInit()
     {
          vkDestroyPipelineLayout(GetGlobalDevice(), pipeline_layout_, g_host_alloc);
     }

     void RHIResourceBindlessSetVulkan::Bind(RHICommandContext::Ptr command)
     {
          /*
          const auto cmd_flags = command->GetFlags();
          VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
          if (cmd_flags == RHICommandContext::EFlags::eAsyncCompute) {
               bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
          }
          */
     }
     RHIResourceHandle RHIResourceBindlessSetVulkan::WriteBuffer(RHIBuffer::Ptr buffer)
     {
          auto buffer_vulkan = dynamic_cast<RHIBufferVulkan*>(buffer);
          const auto tag = ConvertBufferDescToBindTag(buffer->GetBufferDesc());
          const auto buffer_handle = GetAvailableResourceHandle(tag);
          const auto set_index = GetDescriptorHeapIndex(tag);
          auto& set = *descriptor_heaps_[set_index].set_;
          set.BeginUpdates();
          set[uint32_t(buffer_handle)] = *buffer_vulkan->GetImpl();
          set.EndUpdates();
          return buffer_handle;
     }
     RHIResourceHandle RHIResourceBindlessSetVulkan::WriteTexture(RHITexture::Ptr texture)
     {
          auto texture_vulkan = dynamic_cast<RHITextureVulkan*>(texture);
          const auto tag = ConvertTextureDescToBindTag(texture->GetTextureDesc());
          const auto texture_handle = GetAvailableResourceHandle(tag);
          const auto set_index = GetDescriptorHeapIndex(tag);
          auto& set = *descriptor_heaps_[set_index].set_;
          set.BeginUpdates();
          set[uint32_t(texture_handle)] = *texture_vulkan->GetImpl();
          set.EndUpdates();
          return texture_handle;
     }
     void RHIResourceBindlessSetVulkan::Notify(const RHINotifyHeader& header, const Span<uint8_t>& notify_data)
     {
          PCHECK(IsBind()) << "resource bindless is binded";
          RHIPushConstant cmd_packet;
          cmd_packet.flags_ = header.flags_;
          cmd_packet.user_data_ = notify_data;
          cmd_packet.offset_ = header.offset_;
          cmd_buffer_->Enqueue(&cmd_packet);
     }
     uint32_t RHIResourceBindlessSetVulkan::GetDescriptorHeapIndex(uint32_t tag_flags)
     {
          return tag_set_index_[tag_flags];
     }
     
     void RHIResourceBindlessSetVulkan::CreateDescriptorHeap(const RHIBindLessTableInitializer::Member& desc, VulkanPipelineLayoutDesc& pipe_desc)
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
