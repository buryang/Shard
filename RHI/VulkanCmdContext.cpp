#include "VulkanCmdContext.h"
#include "VulkanRenderPass.h"
#include "VulkanRayTraceKHR.h"

namespace MetaInit
{

	VkCommandBufferAllocateInfo MakeCommandBufferAllocateInfo(VkCommandPool pool, VkCommandBufferLevel level, uint32_t buffer_count)
	{
		VkCommandBufferAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		alloc_info.pNext = VK_NULL_HANDLE;
		alloc_info.commandPool = pool;
		alloc_info.level = level;
		alloc_info.commandBufferCount = buffer_count;
		return alloc_info;
	}

	VkCommandBufferBeginInfo MakeCommandBufferBeginInfo(VkCommandBufferUsageFlags flags)
	{

	}

	VkCommandPoolCreateInfo MakeCommandPoolCreateInfo(VkCommandPoolCreateFlags flags, uint32_t family_index)
	{

	}

	VulkanCmdBuffer::Ptr VulkanCmdBuffer::Create(const VkCommandBufferAllocateInfo& cmd_info, VulkanCmdPool& cmd_pool)
	{

	}

	VulkanCmdBuffer::~VulkanCmdBuffer()
	{
		if (VK_NULL_HANDLE != buffer_)
		{
			Reset();
		}
	}

	void VulkanCmdBuffer::Begin()
	{
		VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		begin_info.pNext = VK_NULL_HANDLE;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = VK_NULL_HANDLE;
		vkBeginCommandBuffer(buffer_, &begin_info);
	}

	void VulkanCmdBuffer::End()
	{
		vkEndCommandBuffer(buffer_);
	}

	void VulkanCmdBuffer::BeginPass(VulkanRenderPass& render_pass)
	{
		render_pass.Begin(*this);
	}

	void VulkanCmdBuffer::EndPass(VulkanRenderPass& render_pass)
	{
		render_pass.End(*this);
	}

	void VulkanCmdBuffer::Dispatch()
	{
		//FIXME
		vkCmdDispatch(buffer_, 0, 0, 0);
	}

	void VulkanCmdBuffer::Draw( uint32_t first_instance, uint32_t instance_count, uint32_t first_vertex, uint32_t vertex_count)
	{
		vkCmdDraw(buffer_, vertex_count, instance_count, first_vertex, first_instance);
	}

	void VulkanCmdBuffer::TraceRay(std::unordered_map<uint32_t, VulkanRayTraceBindTable>& ray_binds, const glm::uvec3& dims)
	{
		vkCmdTraceRaysKHR(buffer_, ray_binds[static_cast<uint32_t>(VulkanRayTraceBindTable::Type::RGEN)],
			ray_binds[static_cast<uint32_t>(VulkanRayTraceBindTable::Type::RMISS)],
			ray_binds[static_cast<uint32_t>(VulkanRayTraceBindTable::Type::RHIT)],
			ray_binds[static_cast<uint32_t>(VulkanRayTraceBindTable::Type::RCALL)],
			dims.x, dims.y, dims.z);
	}
	void VulkanCmdBuffer::Submit(VkQueue& queue)
	{
		//FIXME
		VkSubmitInfo submit_info{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit_info.pNext				= VK_NULL_HANDLE;
		submit_info.commandBufferCount	= 1;
		submit_info.pCommandBuffers		= &buffer_;
		auto ret = vkQueueSubmit(queue, 1, &submit_info, nullptr);
		assert(ret == VK_SUCCESS && "submit command buffer failed");
	}

	void VulkanCmdBuffer::Execute(Vector<VulkanCmdBuffer>& cmd_buffers)
	{
		auto cmd_count = cmd_buffers.size();
		Vector<VkCommandBuffer> cmd_vec(cmd_count);
		auto vec_iter = cmd_vec.begin();
		auto buffer_iter = cmd_buffers.begin();
		for (;vec_iter != cmd_vec.end(); ++vec_iter)
		{
			*vec_iter = buffer_iter->buffer_;
		}
		vkCmdExecuteCommands(buffer_, static_cast<uint32_t>(cmd_count), cmd_vec.data());
	}

	void VulkanCmdBuffer::Reset()
	{
		vkResetCommandBuffer(buffer_, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
	}
}