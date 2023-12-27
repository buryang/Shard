#include "RHISyncVulkan.h"

namespace Shard::RHI::Vulkan {

     void RHICreateTransition(const Renderer::RtBarrierBatch& barrier_info, RHITransitionInfo::Ptr& trans)
     {
          auto vulkan_info = trans->GetMetaData<VulkanTransitionInfo>();
          for (auto& trans : barrier_info.transitions_) {

          }
     }

     void RHIBeginTransition(RHICommandContext::Ptr cmd, const RHITransitionInfo::Ptr trans)
     {

     }

     void RHIEndTransition(RHICommandContext::Ptr cmd, const RHITransitionInfo::Ptr trans)
     {

     }

     RHIEventVulkan::RHIEventVulkan(const RHIEventInitializer& initializer):RHIEvent(initializer)
     {
          VkEventCreateInfo create_info{
               .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
               .pNext = nullptr,
               .flags = initializer.flags_
          };
          auto ret = vkCreateEvent(GetGlobalDevice(), &create_info, g_host_alloc, &handle_);
          PCHECK(ret == VK_SUCCESS) << "failed to create vulkan event";
     }

     RHIEventVulkan::~RHIEventVulkan()
     {
          if (handle_ != VK_NULL_HANDLE) {
               vkDestroyEvent(GetGlobalDevice(), handle_, g_host_alloc);
          }
     }
}
