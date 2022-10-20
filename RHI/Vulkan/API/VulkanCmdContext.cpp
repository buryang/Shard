#include "RHI/VulkanCmdContext.h"
#include "RHI/VulkanRenderPass.h"
#include "RHI/VulkanRayTraceKHR.h"
#include "RHI/VulkanPrimitive.h"

namespace MetaInit
{
	using namespace Primitive;
	/*copy from falcor*/
	static inline VkPipelineStageFlags ResourceStateToStageFlags(Primitive::EResourceState state, bool is_src)
	{
		switch (state)
		{
		case Primitive::EResourceState::eUndefined:
		case Primitive::EResourceState::ePreInitialized:
		case Primitive::EResourceState::eCommon:
			return is_src ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : (VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		case Primitive::EResourceState::eCopySrc:
		case Primitive::EResourceState::eCopyDst:
		case Primitive::EResourceState::eResolveSrc:
		case Primitive::EResourceState::eResolveDst:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case Primitive::EResourceState::eRenderTarget:
			return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		default:
			throw std::invalid_argument("image state not supported now");
		}
	}

	static inline VkImageLayout ResourceStateToImageLayout(Primitive::EResourceState state)
	{
		switch (state)
		{
		case Primitive::EResourceState::eUndefined:
			return VK_IMAGE_LAYOUT_UNDEFINED;
		case Primitive::EResourceState::ePreInitialized:
			return VK_IMAGE_LAYOUT_PREINITIALIZED;
		case Primitive::EResourceState::eCommon:
		case Primitive::EResourceState::eUnorderedAccess:
			return VK_IMAGE_LAYOUT_GENERAL;
		case Primitive::EResourceState::eRenderTarget:
			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		case Primitive::EResourceState::eDepthStencil:
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		case Primitive::EResourceState::eResolveSrc:
		case Primitive::EResourceState::eCopySrc:
			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		case Primitive::EResourceState::eResolveDst:
		case Primitive::EResourceState::eCopyDst:
			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		case Primitive::EResourceState::eShaderResource:
			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		case Primitive::EResourceState::ePresent:
			return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		default:
			throw std::invalid_argument("resource state not supported by image layout transform");
		}
	}

	static inline VkAccessFlags ResourceStateToAccessFlags(Primitive::EResourceState state)
	{
		switch (state)
		{
		case Primitive::EResourceState::eUndefined:
		case Primitive::EResourceState::eCommon:
		case Primitive::EResourceState::ePreInitialized:
		case Primitive::EResourceState::ePresent:
			return static_cast<VkAccessFlags>(0);
		case Primitive::EResourceState::eVertexBuffer:
			return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		case Primitive::EResourceState::eIndexBuffer:
			return VK_ACCESS_INDEX_READ_BIT;
		case Primitive::EResourceState::eShaderResource:
			return VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		case Primitive::EResourceState::eResolveSrc:
		case Primitive::EResourceState::eCopySrc:
			return VK_ACCESS_TRANSFER_READ_BIT;
		case Primitive::EResourceState::eResolveDst:
		case Primitive::EResourceState::eCopyDst:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		case Primitive::EResourceState::eIndirectArgs:
			return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		case Primitive::EResourceState::eUnorderedAccess:
			return VK_ACCESS_SHADER_WRITE_BIT;
		case Primitive::EResourceState::eRenderTarget:
			return VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		case Primitive::EResourceState::eDepthStencil:
			return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		default:
			throw std::invalid_argument("resource state not supported by access flags transform");
		}
	}

	VkCommandBufferAllocateInfo MakeCommandBufferAllocateInfo(VkCommandPool pool, VkCommandBufferLevel level, uint32_t buffer_count)
	{
		VkCommandBufferAllocateInfo alloc_info;
		memset(&alloc_info, sizeof(VkCommandBufferAllocateInfo), 1);
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.pNext = VK_NULL_HANDLE;
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

	VulkanCmdPool::VulkanCmdPool(VulkanDevice::Ptr device, VkCommandPoolCreateFlags flags, uint32_t family_index):device_(device), family_index_(family_index)
	{
		auto create_info = MakeCommandPoolCreateInfo(flags, family_index);
		auto ret = vkCreateCommandPool(device_->Get(), &create_info, g_host_alloc, &pool_);
		if (VK_SUCCESS != ret)
		{
			throw std::runtime_error("create command pool failed");
		}
	}

	bool VulkanCmdPool::IsFull() const
	{
		return false;
	}

	void VulkanCmdPool::Reset()
	{
		vkResetCommandPool(device_->Get(), pool_, 0); //VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT, not release resource
		state_ = EState::eIdle;
	}

	VulkanCmdPool::~VulkanCmdPool()
	{
		if (VK_NULL_HANDLE != pool_)
		{
			vkDestroyCommandPool(device_->Get(), pool_, g_host_alloc);
		}
	}

	VulkanCmdBuffer::Ptr VulkanCmdBuffer::Create(VulkanCmdPool::Ptr cmd_pool)
	{
		VulkanCmdBuffer::Ptr cmd_ptr(new VulkanCmdBuffer(cmd_pool));
		VkFenceCreateInfo fence_info{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		fence_info.pNext = VK_NULL_HANDLE;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		auto ret = vkCreateFence(cmd_pool->GetDevice()->Get(), &fence_info, g_host_alloc, &cmd_ptr->fence_);
		assert(ret == VK_SUCCESS && "create fence failed");
		return cmd_ptr;
	}

	VulkanCmdBuffer::~VulkanCmdBuffer()
	{
		if (VK_NULL_HANDLE != handle_)
		{
			Reset();
			vkDestroyFence(pool_->GetDevice()->Get(), fence_, g_host_alloc);
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

	void VulkanCmdBuffer::DispatchIndirect(Primitive::VulkanBuffer& buffer, const VkDeviceSize offset)
	{
		Barrier(buffer, Primitive::EResourceState::eIndirectArgs);
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

	void VulkanCmdBuffer::Copy(Primitive::VulkanBuffer& dst, uint32_t dst_offset, Primitive::VulkanBuffer& src, uint32_t src_offset, uint32_t size)
	{
		Barrier(src, Primitive::EResourceState::eCopySrc);
		Barrier(dst, Primitive::EResourceState::eCopyDst);
		
		VkBufferCopy buffer_region{};
		buffer_region.dstOffset = dst_offset;
		buffer_region.srcOffset = src_offset;
		buffer_region.size = size;
		vkCmdCopyBuffer(handle_, src.Get(), dst.Get(), 1, &buffer_region);
	}

	void VulkanCmdBuffer::Copy(Primitive::VulkanImage& dst, Primitive::VulkanBuffer& src)
	{
		Barrier(src, Primitive::EResourceState::eCopySrc);
		Barrier(dst, Primitive::EResourceState::eCopyDst);

		VkBufferImageCopy image_region{};
		vkCmdCopyBufferToImage(handle_, src.Get(), dst.Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_region);
	}

	void VulkanCmdBuffer::Resolve(Primitive::VulkanImage& dst, Primitive::VulkanImage& src)
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
		if (state_ == EState::eExecutable)//FIXME
		{
			auto nano_seconds = 33 * 1000 * 1000LL;//copy from ue5
			vkWaitForFences(pool_->GetDevice()->Get(), 1, &fence_, true, nano_seconds);
		}
		vkResetCommandBuffer(handle_, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
	}

	void VulkanCmdBuffer::Barrier(Primitive::VulkanImage& image, Primitive::EResourceState new_state)
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

	void VulkanCmdBuffer::Barrier(Primitive::VulkanBuffer& buffer, Primitive::EResourceState new_state)
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
	void VulkanCmdBuffer::Barrier(const VulkanBarrierInfo& barrier_info)
	{
		vkCmdPipelineBarrier(handle_, barrier_info.src_stage_flags_, barrier_info.dst_stage_flags_, barrier_info.depend_flags_,
			barrier_info.mem_barrier_.size(), barrier_info.mem_barrier_.size() > 0 ? barrier_info.mem_barrier_.data() : nullptr,
			barrier_info.buffer_barrier_.size(), barrier_info.buffer_barrier_.size() > 0 ? barrier_info.buffer_barrier_.data() : nullptr,
			barrier_info.image_barrier_.size(), barrier_info.image_barrier_.size() > 0 ? barrier_info.image_barrier_.data() : nullptr);
	}

	VulkanCmdBuffer::VulkanCmdBuffer(VulkanCmdPool::Ptr cmd_pool):pool_(cmd_pool)
	{
		//first to support primary command
		auto& cmd_info = MakeCommandBufferAllocateInfo(pool_->Get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
		auto ret = vkAllocateCommandBuffers(pool_->GetDevice()->Get(), &cmd_info, &handle_);
		if(VK_SUCCESS != ret)
		{
			throw std::runtime_error("alloc command buffer failed");
		}
		state_ = EState::eInitial;
	}

	VulkanCmdPool::Ptr VulkanCmdPoolManager::GetIdlePool(uint32_t family_index)
	{
		VulkanCmdPool::Ptr pool_ptr;
		std::lock_guard<std::mutex> lock(mutex_);
		{
			auto check_pool = [&](const VulkanCmdPool::Ptr pool) {
				return pool->FamilyIndex() == family_index && pool->State() == VulkanCmdPool::EState::eIdle;
			};
			
			auto iter = std::find_if(pools_.begin(), pools_.end(), check_pool);
			if (pools_.end() == iter)
			{
				pool_ptr = std::make_shared<VulkanCmdPool>(new VulkanCmdPool(device_, 0, family_index));
				pools_.push_back(pool_ptr);
			}
			else
			{
				pool_ptr = *iter;
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

	void VulkanCmdPoolManager::Init(VulkanDevice::Ptr device)
	{
		std::lock_guard<std::mutex> lock(read_mutex_);
		if (!is_inited_) {
			device_ = device;
			is_inited_ = true;
		}
	}

}