#include "RHI/Vulkan/API/VulkanCmdContext.h"
#include "RHI/Vulkan/API/VulkanRenderPass.h"
#include "RHI/Vulkan/API/VulkanRayTraceKHR.h"
#include "RHI/Vulkan/API/VulkanResources.h"

namespace Shard
{
     /*copy from falcor*/
     static inline VkPipelineStageFlags ResourceStateToStageFlags(EResourceState state, bool is_src)
     {
          switch (state)
          {
          case EResourceState::eUndefined:
          case EResourceState::ePreInitialized:
          case EResourceState::eCommon:
               return is_src ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : (VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
          case EResourceState::eCopySrc:
          case EResourceState::eCopyDst:
          case EResourceState::eResolveSrc:
          case EResourceState::eResolveDst:
               return VK_PIPELINE_STAGE_TRANSFER_BIT;
          case EResourceState::eRenderTarget:
               return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
          default:
               PLOG(ERROR) << "image state not supported now";
          }
     }

     static inline VkImageLayout ResourceStateToImageLayout(EResourceState state)
     {
          switch (state)
          {
          case EResourceState::eUndefined:
               return VK_IMAGE_LAYOUT_UNDEFINED;
          case EResourceState::ePreInitialized:
               return VK_IMAGE_LAYOUT_PREINITIALIZED;
          case EResourceState::eCommon:
          case EResourceState::eUnorderedAccess:
               return VK_IMAGE_LAYOUT_GENERAL;
          case EResourceState::eRenderTarget:
               return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
          case EResourceState::eDepthStencil:
               return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
          case EResourceState::eResolveSrc:
          case EResourceState::eCopySrc:
               return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
          case EResourceState::eResolveDst:
          case EResourceState::eCopyDst:
               return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
          case EResourceState::eShaderResource:
               return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
          case EResourceState::ePresent:
               return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
          default:
               PLOG(ERROR)<<"resource state not supported by image layout transform";
          }
     }

     static inline VkAccessFlags ResourceStateToAccessFlags(EResourceState state)
     {
          switch (state)
          {
          case EResourceState::eUndefined:
          case EResourceState::eCommon:
          case EResourceState::ePreInitialized:
          case EResourceState::ePresent:
               return static_cast<VkAccessFlags>(0);
          case EResourceState::eVertexBuffer:
               return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
          case EResourceState::eIndexBuffer:
               return VK_ACCESS_INDEX_READ_BIT;
          case EResourceState::eShaderResource:
               return VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
          case EResourceState::eResolveSrc:
          case EResourceState::eCopySrc:
               return VK_ACCESS_TRANSFER_READ_BIT;
          case EResourceState::eResolveDst:
          case EResourceState::eCopyDst:
               return VK_ACCESS_TRANSFER_WRITE_BIT;
          case EResourceState::eIndirectArgs:
               return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
          case EResourceState::eUnorderedAccess:
               return VK_ACCESS_SHADER_WRITE_BIT;
          case EResourceState::eRenderTarget:
               return VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
          case EResourceState::eDepthStencil:
               return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
          default:
               PLOG(ERROR) <<"vulkan resource state not supported by access flags transform" << std::endl;
          }
     }

     VkCommandBufferAllocateInfo MakeCommandBufferAllocateInfo(VkCommandPool pool, VkCommandBufferLevel level, uint32_t buffer_count=1)
     {
          VkCommandBufferAllocateInfo alloc_info;
          memset(&alloc_info, sizeof(VkCommandBufferAllocateInfo), 1);
          alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
          alloc_info.commandPool = pool;
          alloc_info.level = level;
          alloc_info.commandBufferCount = buffer_count;
          return alloc_info;
     }

     VkCommandBufferBeginInfo MakeCommandBufferBeginInfo(VkCommandBufferUsageFlags flags)
     {
          VkCommandBufferBeginInfo begin_info;
          memset(&begin_info, sizeof(VkCommandBufferBeginInfo), 1);
          begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
          begin_info.pNext = VK_NULL_HANDLE;
          begin_info.flags = flags;
          return begin_info;
     }

     VkCommandPoolCreateInfo MakeCommandPoolCreateInfo(VkCommandPoolCreateFlags flags, uint32_t family_index)
     {
          VkCommandPoolCreateInfo pool_info;
          memset(&pool_info, sizeof(VkCommandPoolCreateInfo), 1);
          pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
          pool_info.flags = flags;
          pool_info.pNext = VK_NULL_HANDLE;
          pool_info.queueFamilyIndex = family_index;
          return pool_info;
     }

     VulkanCmdPool::VulkanCmdPool(VkCommandPoolCreateFlags flags, uint32_t family_index):family_index_(family_index)
     {
          auto create_info = MakeCommandPoolCreateInfo(flags, family_index);
          auto ret = vkCreateCommandPool(GetGlobalDevice(), &create_info, g_host_alloc, &pool_);
          PCHECK(VK_SUCCESS == ret) << "vulkan create command pool failed" << std::endl;
     }

     bool VulkanCmdPool::IsFull() const
     {
          return false;
     }

     void VulkanCmdPool::Reset()
     {
          vkResetCommandPool(GetGlobalDevice(), handle_, 0); //VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT, not release resource
          state_ = EState::eIdle;
     }

     VulkanCmdPool::~VulkanCmdPool()
     {
          if (VK_NULL_HANDLE != handle_)
          {
               vkDestroyCommandPool(GetGlobalDevice(), handle_, g_host_alloc);
          }
     }

     VulkanCmdBuffer::SharedPtr VulkanCmdBuffer::Create(VulkanCmdPool::SharedPtr cmd_pool)
     {
          VulkanCmdBuffer::SharedPtr cmd_ptr(new VulkanCmdBuffer(cmd_pool));
          VkFenceCreateInfo fence_info{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
          fence_info.pNext = VK_NULL_HANDLE;
          fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
          auto ret = vkCreateFence(GetGlobalDevice(), &fence_info, g_host_alloc, &cmd_ptr->fence_);
          PCHECK(ret == VK_SUCCESS) << "create fence failed";
          return cmd_ptr;
     }

     VulkanCmdBuffer::~VulkanCmdBuffer()
     {
          if (VK_NULL_HANDLE != handle_)
          {
               Reset();
               vkDestroyFence(GetGlobalDevice(), fence_, g_host_alloc);
          }
     }

     void VulkanCmdBuffer::SetState(EState state)
     {
          state_ = state;
     }

     VulkanCmdBuffer::EState VulkanCmdBuffer::State() const
     {
          return state_;
     }

     void VulkanCmdBuffer::Begin()
     {
          VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
          begin_info.pNext = VK_NULL_HANDLE;
          //In general approaches that pre-record command buffers for parts of the scene are counter-productive
          begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
          begin_info.pInheritanceInfo = VK_NULL_HANDLE;
          vkBeginCommandBuffer(handle_, &begin_info);
          state_ = EState::eRecording;
     }

     void VulkanCmdBuffer::End()
     {
          vkEndCommandBuffer(handle_);
          state_ = VulkanCmdBuffer::EState::eExecutable;
     }

     void VulkanCmdBuffer::Dispatch(vec3 group_size)
     {
          vkCmdDispatch(handle_, group_size.x, group_size.y, group_size.z);
     }

     void VulkanCmdBuffer::DispatchIndirect( VulkanBuffer& buffer, const VkDeviceSize offset)
     {
          Barrier(buffer, EResourceState::eIndirectArgs);
          vkCmdDispatchIndirect(handle_, buffer.Get(), offset);
     }

     //unreal engine use 0 as default first instance 
     void VulkanCmdBuffer::Draw( uint32_t first_instance, uint32_t instance_count, uint32_t first_vertex, uint32_t vertex_count)
     {
          vkCmdDraw(handle_, vertex_count, instance_count, first_vertex, first_instance);
     }

     void VulkanCmdBuffer::DrawIndirect(const VulkanBuffer& buffer, uint32_t offset, uint32_t draw_count, uint32_t stride)
     {
          vkCmdDrawIndirect(handle_, buffer.Get(), static_cast<VkDeviceSize>(offset), draw_count, stride);
     }

     void VulkanCmdBuffer::DrawIndexed(uint32_t first_instance, uint32_t instance_count, uint32_t vertex_offset, uint32_t first_index, uint32_t index_count)
     {
          instance_count = std::max(1U, instance_count);
          vkCmdDrawIndexed(handle_, index_count, instance_count, first_index, vertex_offset, first_instance);
     }

     void VulkanCmdBuffer::DrawIndexedIndirect(const VulkanBuffer& buffer, uint32_t offset, uint32_t draw_count, uint32_t stride)
     {
          vkCmdDrawIndexedIndirect(handle_, buffer.Get(), static_cast<VkDeviceSize>(offset), draw_count, stride);
     }

     void VulkanCmdBuffer::TraceRay(Map<uint32_t, VulkanRayTraceBindTable>& ray_binds, const glm::uvec3& dims)
     {
          /*
          vkCmdTraceRaysKHR(buffer_, ray_binds[static_cast<uint32_t>(VulkanRayTraceBindTable::Type::RGEN)],
               ray_binds[static_cast<uint32_t>(VulkanRayTraceBindTable::Type::RMISS)],
               ray_binds[static_cast<uint32_t>(VulkanRayTraceBindTable::Type::RHIT)],
               ray_binds[static_cast<uint32_t>(VulkanRayTraceBindTable::Type::RCALL)],
               dims.x, dims.y, dims.z);
          */
     }

     void VulkanCmdBuffer::Copy( VulkanBuffer& dst, uint32_t dst_offset,  VulkanBuffer& src, uint32_t src_offset, uint32_t size)
     {
          Barrier(src, EResourceState::eCopySrc);
          Barrier(dst, EResourceState::eCopyDst);
          
          VkBufferCopy buffer_region{};
          buffer_region.dstOffset = dst_offset;
          buffer_region.srcOffset = src_offset;
          buffer_region.size = size;
          vkCmdCopyBuffer(handle_, src.Get(), dst.Get(), 1, &buffer_region);
     }

     void VulkanCmdBuffer::Copy( VulkanImage& dst,  VulkanBuffer& src)
     {
          Barrier(src, EResourceState::eCopySrc);
          Barrier(dst, EResourceState::eCopyDst);

          VkBufferImageCopy image_region{};
          vkCmdCopyBufferToImage(handle_, src.Get(), dst.Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_region);
     }

     void VulkanCmdBuffer::Resolve( VulkanImage& dst,  VulkanImage& src)
     {
          VkImageSubresourceRange src_range, dst_range;
          //todo 
          VkImageResolve resolve_info{};
          //resolve_info.srcSubresource = src_range;
          //resolve_info.dstSubresource = dst_range;
          vkCmdResolveImage(handle_, src.Get(), ResourceStateToImageLayout(src.GetState()), 
                                             dst.Get(), ResourceStateToImageLayout(dst.GetState()), 1, &resolve_info);
     }

     void VulkanCmdBuffer::Execute(Vector<VulkanCmdBuffer>& cmd_buffers)
     {
          auto cmd_count = cmd_buffers.size();
          Vector<VkCommandBuffer> cmd_vec(cmd_count);
          auto vec_iter = cmd_vec.begin();
          auto buffer_iter = cmd_buffers.begin();
          for (;vec_iter != cmd_vec.end(); ++vec_iter)
          {
               *vec_iter = buffer_iter->handle_;
          }
          vkCmdExecuteCommands(handle_, static_cast<uint32_t>(cmd_count), cmd_vec.data());
          state_ = EState::ePending;
     }

     void VulkanCmdBuffer::Reset()
     { 
          /* 
               VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows any command buffer allocated from a
               pool to be individually reset to the initial state; either by calling vkResetCommandBuffer, or via
               the implicit reset when calling vkBeginCommandBuffer.If this flag is not set on a pool, then
               vkResetCommandBuffer must not be called for any command buffer allocated from that pool.
          */
          if (parent_->GetFlags() == VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) {
               if (state_ == EState::eExecutable)//FIXME
               {
                    auto nano_seconds = 33 * 1000 * 1000LL;//copy from ue5
                    vkWaitForFences(GetGlobalDevice(), 1, &fence_, true, nano_seconds);
               }
               vkResetCommandBuffer(handle_, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
          }
     }

     void VulkanCmdBuffer::Barrier( VulkanImage& image, EResourceState new_state)
     {
          VulkanBarrierInfo barrier_info{};
          barrier_info.src_stage_flags_ = ResourceStateToStageFlags(image.GetState(), true);
          barrier_info.dst_stage_flags_ = ResourceStateToStageFlags(new_state, false);
          VkImageMemoryBarrier image_barrier{};
          //todo fill image barrier
          memset(&image_barrier, 0, sizeof(image_barrier));
          image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
          image_barrier.image = image.Get();
          image_barrier.oldLayout = ResourceStateToImageLayout(image.GetState());
          image_barrier.newLayout = ResourceStateToImageLayout(new_state);
          image_barrier.srcAccessMask = ResourceStateToAccessFlags(image.GetState());
          image_barrier.dstAccessMask = ResourceStateToAccessFlags(new_state);
          image_barrier.subresourceRange = 0;//todo 
          barrier_info.image_barrier_.emplace_back(image_barrier);
          Barrier(barrier_info); //memory bugs  to fix
          image.SetState(new_state);
     }

     void VulkanCmdBuffer::Barrier( VulkanBuffer& buffer, EResourceState new_state)
     {
          VulkanBarrierInfo barrier_info{};
          barrier_info.src_stage_flags_ = ResourceStateToStageFlags(buffer.GetState(), true);
          barrier_info.dst_stage_flags_ = ResourceStateToStageFlags(new_state, false);
          VkBufferMemoryBarrier buffer_barrier{};
          barrier_info.buffer_barrier_.emplace_back(buffer_barrier);
          Barrier(barrier_info); //memory bugs  to fix
          buffer.SetState(new_state);
     }

     //barriers need to be batched as aggressively as possible
     //https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
     void VulkanCmdBuffer::Barrier(const RHI::VulkanTransitionInfo& barrier_info)
     {
          vkCmdPipelineBarrier(handle_, barrier_info.src_mask_, barrier_info.dst_mask_, barrier_info.depend_flags_,
               barrier_info, barrier_info.mem_barrier_.size() > 0 ? barrier_info.mem_barrier_.data() : nullptr,
               barrier_info.buffer_barrier_.size(), barrier_info.buffer_barrier_.size() > 0 ? barrier_info.buffer_barrier_.data() : nullptr,
               barrier_info.image_barriers_.size(), barrier_info.image_barriers_.size() > 0 ? barrier_info.image_barriers_.data() : nullptr);
     }

     void VulkanCmdBuffer::PushConstants(const uint32_t flags, const uint32_t offset, const Span<uint8_t>& constants)
     {
          vkCmdPushConstants(handle_, nullptr, flags, offset, constants.size_bytes(), constants.data());
     }

     void VulkanCmdBuffer::SignalEvent(VkEvent event)
     {
          vkCmdSetEvent(handle_, event, 0u);//todo
     }

     void VulkanCmdBuffer::WaitEvenets(VkEvent* events, uint32_t count)
     {
          vkCmdWaitEvents(handle_, count, events, 0, 0, 0, nullptr, 0, nullptr, 0, nullptr);//todo
     }

     VulkanCmdBuffer::VulkanCmdBuffer(VulkanCmdPool::SharedPtr cmd_pool):parent_(cmd_pool)
     {
          //first to support primary command
          const auto& cmd_info = MakeCommandBufferAllocateInfo(cmd_pool->Get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
          auto ret = vkAllocateCommandBuffers(GetGlobalDevice(), &cmd_info, &handle_);
          PCHECK(VK_SUCCESS != ret) << "alloc command buffer failed";
          state_ = EState::eInitial;
     }

     VulkanCmdPool::SharedPtr VulkanCmdPoolManager::GetIdlePool(VkCommandPoolCreateFlags flags, uint32_t family_index)
     {
          VulkanCmdPool::SharedPtr pool_ptr;
          std::scoped_lock <std::mutex> lock(mutex_);
          {
               auto check_pool = [&](const auto pool)->bool {
                    return pool->State() == VulkanCmdPool::EState::eIdle;
               };

               uint64_t key = (static_cast<uint32_t>(flags) << 32) | family_index;
               if (auto iter = pools_.find(key); iter != pools_.end())
               {
                    auto family_pools = iter->second;
                    auto pool_iter = eastl::find_if(family_pools.begin(), family_pools.end(), check_pool);
                    if (pool_iter != family_pools.end()) {
                         pool_ptr = *pool_iter;
                    }
                    else
                    {
                         pool_ptr.reset(new VulkanCmdPool(flags, family_index));
                         pools_[key].push_back(pool_ptr);
                    }
               }
               else
               {
                    pool_ptr.reset(new VulkanCmdPool(flags, family_index));
                    pools_[key].push_back(pool_ptr);
               }
               pool_ptr->SetState(VulkanCmdPool::EState::eUsing);
          }
          return pool_ptr;
     }

     VulkanCmdPoolManager& VulkanCmdPoolManager::Instance()
     {
          static VulkanCmdPoolManager pool_manager;
          return pool_manager;
     }
}