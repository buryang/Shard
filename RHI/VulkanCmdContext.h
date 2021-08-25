#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/VulkanRHI.h"
#include "RHI/VulkanPrimitive.h"
#include <unordered_map>
#include <mutex>


namespace MetaInit
{
	VkCommandBufferAllocateInfo MakeCommandBufferAllocateInfo(VkCommandPool pool, VkCommandBufferLevel level, uint32_t buffer_count);
	VkCommandBufferBeginInfo MakeCommandBufferBeginInfo(VkCommandBufferUsageFlags flags);
	VkCommandPoolCreateInfo MakeCommandPoolCreateInfo(VkCommandPoolCreateFlags flags, uint32_t family_index);
	
	class VulkanRayTraceBindTable;
	class VulkanCmdBuffer;
	class VulkanCmdPool
	{
	public:
		using Ptr = std::shared_ptr<VulkanCmdPool>;
		explicit VulkanCmdPool(VulkanDevice::Ptr device, VkCommandPoolCreateFlags flags, uint32_t family_index);
		DISALLOW_COPY_AND_ASSIGN(VulkanCmdPool);
		VkCommandPool Get() { return pool_; }
		VulkanDevice::Ptr GetDevice() { return device_; }
		void Reset();
		virtual ~VulkanCmdPool();
	private:
		void CreateBuffers();
	private:
		VulkanDevice::Ptr			device_;
		VkCommandPool				pool_{ VK_NULL_HANDLE };
		Vector<VulkanCmdBuffer>		buffers_;
		std::mutex					mutex_;
	};

	typedef struct VulkanBarrierInfo
	{
		VkPipelineStageFlags			src_stage_flags_;
		VkPipelineStageFlags			dst_stage_flags_;
		VkDependencyFlags				depend_flags_;
		Vector<VkMemoryBarrier>			mem_barrier_;
		Vector<VkBufferMemoryBarrier>	buffer_barrier_;
		Vector<VkImageMemoryBarrier>	image_barrier_;
	}VulkanBarrierInfo;

	class VulkanRenderPass;
	class VulkanCmdBuffer
	{
	public:
		using Ptr = std::unique_ptr<VulkanCmdBuffer>;
		static Ptr Create(VkCommandBuffer cmd_buffer, VulkanCmdPool::Ptr cmd_pool);
		~VulkanCmdBuffer();
		VkCommandBuffer Get()const{
			return handle_;
		}
		/*cmd buffer type*/
		enum class EType :uint8_t
		{
			ePrimary,
			eSecondary,
			eCount
		};
		/*cmd buffer state*/
		enum class EState:uint32_t
		{
			eInitial,
			eRecording,
			eExecutable,
			ePending,
			eInvalid,
			eCount
		};
		//void SetViewPoint(mat4 view_matrix);
		//void SetLight(vec3 energy, vec3 position, vec3 direction);
		void SetState(EState state);
		EState State()const;
		void Begin();
		void End();
		//compute pipeline must be bound to a command buffer before any dispatch commands are recorded
		void Dispatch(vec3 group_size);
		void DispatchIndirect(Primitive::VulkanBuffer& buffer, const VkDeviceSize offset);
		void Draw(uint32_t first_instance, uint32_t instance_count, uint32_t first_vertex, uint32_t vertex_count);		
		void TraceRay(std::unordered_map<uint32_t, VulkanRayTraceBindTable>& ray_binds, const glm::uvec3& dims);
		void Copy(Primitive::VulkanBuffer& dst, uint32_t dst_offset, Primitive::VulkanBuffer& src, uint32_t src_offset, uint32_t size);
		void Copy(Primitive::VulkanImage& dst, Primitive::VulkanBuffer& src);
		void Resolve(Primitive::VulkanImage& dst, Primitive::VulkanImage& src);
		void Execute(Vector<VulkanCmdBuffer>& cmd_buffers);
		void Reset();
		void Barrier(Primitive::VulkanImage& image, Primitive::EResourceState new_state);
		void Barrier(Primitive::VulkanBuffer& buffer, Primitive::EResourceState new_state);
		VkCommandBuffer Get() { return handle_; }
	private:
		DISALLOW_COPY_AND_ASSIGN(VulkanCmdBuffer);
		explicit VulkanCmdBuffer(VkCommandBuffer cmd_buffer, VulkanCmdPool::Ptr cmd_pool);
		void Barrier(const VulkanBarrierInfo& barrier_info);
	private:
		friend class VulkanCmdPool;
		friend class VulkanQueue;
		VkCommandBuffer		handle_{ VK_NULL_HANDLE };
		VkFence				fence_{ VK_NULL_HANDLE };
		VkFlags				flags_ = 0;
		VulkanCmdPool::Ptr  pool_;
		EState				state_{ EState::INITIAL };
		EType				type_{ EType::PRIMARY };
	};

	class VulkanCmdPoolManager
	{
	public:
		using Ptr = std::shared_ptr<VulkanCmdPoolManager>;
		VulkanCmdPoolManager(VulkanDevice::Ptr device, const uint32_t pool_size);
		VulkanCmdPool GetIdlePool();
	private:
		Vector<VulkanCmdPool::Ptr>	pools_;
	};

}
