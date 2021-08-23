#include "RHI/VulkanCmdContext.h"
#include "RHI/VulkanRenderPass.h"
#include "RHI/VulkanRayTraceKHR.h"

namespace MetaInit
{

	/*copy from falcor*/
	static inline VkPipelineStageFlags ResourceStateToStageFlags(Primitive::EResourceState state, bool is_src)
	{
		switch (state)
		{
		case Primitive::EResourceState::UNDEFINED:
		case Primitive::EResourceState::PREINITIALIZED:
		case Primitive::EResourceState::COMMON:
			return is_src ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : (VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		case Primitive::EResourceState::COPY_SRC:
		case Primitive::EResourceState::COPY_DST:
		case Primitive::EResourceState::RESOLVE_SRC:
		case Primitive::EResourceState::RESOLVE_DST:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case Primitive::EResourceState::RENDER_TARGET:
			return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		default:
			throw std::invalid_argument("image state not supported now");
		}
	}

	static inline VkImageLayout ResourceStateToImageLayout(Primitive::EResourceState state)
	{
		switch (state)
		{
		case Primitive::EResourceState::UNDEFINED:
			return VK_IMAGE_LAYOUT_UNDEFINED;
		case Primitive::EResourceState::PREINITIALIZED:
			return VK_IMAGE_LAYOUT_PREINITIALIZED;
		case Primitive::EResourceState::COMMON:
		case Primitive::EResourceState::UNORDERED_ACCESS:
			return VK_IMAGE_LAYOUT_GENERAL;
		case Primitive::EResourceState::RENDER_TARGET:
			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		case Primitive::EResourceState::DEPTH_STENCIL:
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		case Primitive::EResourceState::RESOLVE_SRC:
		case Primitive::EResourceState::COPY_SRC:
			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		case Primitive::EResourceState::RESOLVE_DST:
		case Primitive::EResourceState::COPY_DST:
			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		case Primitive::EResourceState::SHADER_RESOURCE:
			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		case Primitive::EResourceState::PRESENT:
			return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		default:
			throw std::invalid_argument("resource state not supported by image layout transform");
		}
	}

	static inline VkAccessFlags ResourceStateToAccessFlags(Primitive::EResourceState state)
	{
		switch (state)
		{
		case Primitive::EResourceState::UNDEFINED:
		case Primitive::EResourceState::COMMON:
		case Primitive::EResourceState::PREINITIALIZED:
		case Primitive::EResourceState::PRESENT:
			return static_cast<VkAccessFlags>(0);
		case Primitive::EResourceState::VERTEX_BUFFER:
			return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		case Primitive::EResourceState::INDEX_BUFFER:
			return VK_ACCESS_INDEX_READ_BIT;
		case Primitive::EResourceState::SHADER_RESOURCE:
			return VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		case Primitive::EResourceState::RESOLVE_SRC:
		case Primitive::EResourceState::COPY_SRC:
			return VK_ACCESS_TRANSFER_READ_BIT;
		case Primitive::EResourceState::RESOLVE_DST:
		case Primitive::EResourceState::COPY_DST:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		case Primitive::EResourceState::INDIRECT_ARGS:
			return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		case Primitive::EResourceState::UNORDERED_ACCESS:
			return VK_ACCESS_SHADER_WRITE_BIT;
		case Primitive::EResourceState::RENDER_TARGET:
			return VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		case Primitive::EResourceState::DEPTH_STENCIL:
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

	VulkanCmdPool::VulkanCmdPool(VulkanDevice::Ptr device, VkCommandPoolCreateFlags flags, uint32_t family_index):device_(device)
	{
		auto create_info = MakeCommandPoolCreateInfo(flags, family_index);
		auto ret = vkCreateCommandPool(device_->Get(), &create_info, g_host_alloc, &pool_);
		if (VK_SUCCESS != ret)
		{
			throw std::runtime_error("create command pool failed");
		}
	}

	void VulkanCmdPool::Reset()
	{
		vkResetCommandPool(device_->Get(), pool_, 0); //VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT, not release resource
	}

	VulkanCmdPool::~VulkanCmdPool()
	{
		if (VK_NULL_HANDLE != pool_)
		{
			vkDestroyCommandPool(device_->Get(), pool_, g_host_alloc);
		}
	}

	VulkanCmdBuffer::Ptr VulkanCmdBuffer::Create(VkCommandBuffer cmd_buffer, VulkanCmdPool::Ptr cmd_pool)
	{
		VulkanCmdBuffer::Ptr cmd_ptr(new VulkanCmdBuffer(cmd_buffer, cmd_pool));
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
		state_ = EState::RECORDING;
	}

	void VulkanCmdBuffer::ExecuteEnd()
	{
		vkEndCommandBuffer(handle_);
		state_ = EState::EXECUTABLE;
	}

	void VulkanCmdBuffer::Dispatch(vec3 group_size)
	{
		vkCmdDispatch(handle_, group_size.x, group_size.y, group_size.z);
	}

	void VulkanCmdBuffer::DispatchIndirect(Primitive::VulkanBuffer& buffer, const VkDeviceSize offset)
	{
		Barrier(buffer, Primitive::ResourceState::INDIRECT_ARGS);
		vkCmdDispatchIndirect(handle_, buffer.Get(), offset);
	}

	void VulkanCmdBuffer::Draw( uint32_t first_instance, uint32_t instance_count, uint32_t first_vertex, uint32_t vertex_count)
	{
		vkCmdDraw(handle_, vertex_count, instance_count, first_vertex, first_instance);
	}

	void VulkanCmdBuffer::TraceRay(std::unordered_map<uint32_t, VulkanRayTraceBindTable>& ray_binds, const glm::uvec3& dims)
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
		Barrier(src, Primitive::EResourceState::COPY_SRC);
		Barrier(dst, Primitive::EResourceState::COPY_DST);
		
		VkBufferCopy buffer_region{};
		buffer_region.dstOffset = dst_offset;
		buffer_region.srcOffset = src_offset;
		buffer_region.size = size;
		vkCmdCopyBuffer(handle_, src.Get(), dst.Get(), 1, &buffer_region);
	}

	void VulkanCmdBuffer::Copy(Primitive::VulkanImage& dst, Primitive::VulkanBuffer& src)
	{
		Barrier(src, Primitive::EResourceState::COPY_SRC);
		Barrier(dst, Primitive::EResourceState::COPY_DST);

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
		vkCmdResolveImage(handle_, src.Get(), nullptr, dst.Get(), nullptr, 1, &resolve_info);
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
		state_ = EState::PENDING;
	}

	void VulkanCmdBuffer::Reset()
	{ 
		if (state_ == EState::EXECUTABLE)//FIXME
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

	VulkanCmdBuffer::VulkanCmdBuffer(VkCommandBuffer cmd_buffer, VulkanCmdPool::Ptr cmd_pool):handle_(cmd_buffer), pool_(cmd_pool)
	{
		state_ = EState::INITIAL;
	}

	VulkanCmdPoolManager::VulkanCmdPoolManager(VulkanDevice::Ptr device, const uint32_t pool_size)
	{
		pools_.reserve(pool_size);
		for (auto n = 0; n < pool_size; ++n)
		{
			VulkanCmdPool::Ptr cmd_pool(new VulkanCmdPool(device, 0, 0));
			pools_.emplace_back(cmd_pool);
		}

	}

	VulkanCmdPool VulkanCmdPoolManager::GetIdlePool()
	{
		return VulkanCmdPool();
	}

}