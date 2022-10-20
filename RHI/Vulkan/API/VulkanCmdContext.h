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
	VkSemaphoreCreateInfo MakeSemphoreCreateInfo(VkSemaphoreCreateFlags flags = 0x0);
	class VulkanRayTraceBindTable;
	/*
	* Command pools are application-synchronized, meaning that a command pool
	* must not be used concurrently in multiple threads. That includes use via
	* recording commands on any command buffers allocated from the pool, as well
	* operations that allocate, free, and reset command buffers or the pool itself.
	*/
	class VulkanCmdPool
	{
	public:
		enum EState :uint32_t
		{
			eUsing,
			eIdle,
		};
		using Ptr = std::shared_ptr<VulkanCmdPool>;
		explicit VulkanCmdPool(VulkanDevice::Ptr device, VkCommandPoolCreateFlags flags, uint32_t family_index);
		DISALLOW_COPY_AND_ASSIGN(VulkanCmdPool);
		VkCommandPool Get() { return pool_; }
		VulkanDevice::Ptr GetDevice() { return device_; }
		EState State()const { return state_; }
		uint32_t FamilyIndex()const { return family_index_; }
		void SetState(EState state) { state_ = state; } 
		bool IsFull()const;
		void Reset();
		virtual ~VulkanCmdPool();
	private:
		VulkanDevice::Ptr			device_;
		uint32_t					family_index_;
		VkCommandPool				pool_{ VK_NULL_HANDLE };
		EState						state_{ EState::eIdle };
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
	class VulkanBuffer;
	class VulkanCmdBuffer
	{
	public:
		using Ptr = std::unique_ptr<VulkanCmdBuffer>;
		static Ptr Create(VulkanCmdPool::Ptr cmd_pool);
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
		void DrawIndirect(const Primitive::VulkanBuffer& buffer, uint32_t offset, uint32_t draw_count, uint32_t stride);
		void DrawIndexed(uint32_t first_instance, uint32_t instance_count, uint32_t vertex_offset, uint32_t first_index, uint32_t index_count);
		void DrawIndexedIndirect(const Primitive::VulkanBuffer& buffer, uint32_t offset, uint32_t draw_count, uint32_t stride);
		void TraceRay(Map<uint32_t, VulkanRayTraceBindTable>& ray_binds, const glm::uvec3& dims);
		void Copy(Primitive::VulkanBuffer& dst, uint32_t dst_offset, Primitive::VulkanBuffer& src, uint32_t src_offset, uint32_t size);
		void Copy(Primitive::VulkanImage& dst, Primitive::VulkanBuffer& src);
		void Resolve(Primitive::VulkanImage& dst, Primitive::VulkanImage& src);
		void Execute(Vector<VulkanCmdBuffer>& cmd_buffers);
		void Reset();
		void Barrier(Primitive::VulkanImage& image, Primitive::EResourceState new_state);
		void Barrier(Primitive::VulkanBuffer& buffer, Primitive::EResourceState new_state);
		void BeginRenderPass();
		void EndRenderPass();
		void UpdateTopLevel();
		void UPdateBottomLevel();
		void UpdateShaderTable();
		void RayTrace();
		VkCommandBuffer Get() { return handle_; }
	private:
		DISALLOW_COPY_AND_ASSIGN(VulkanCmdBuffer);
		explicit VulkanCmdBuffer(VulkanCmdPool::Ptr cmd_pool);
		void Barrier(const VulkanBarrierInfo& barrier_info);
	private:
		friend class VulkanCmdPool;
		friend class VulkanQueue;
		VkCommandBuffer		handle_{ VK_NULL_HANDLE };
		VkFence				fence_{ VK_NULL_HANDLE };
		VkFlags				flags_ = 0;
		VulkanCmdPool::Ptr  pool_;
		EState				state_{ EState::eInitial };
		EType				type_{ EType::ePrimary };
	};

	class VulkanCmdPoolManager
	{
	public:
		VulkanCmdPoolManager()=default;
		void Init(VulkanDevice::Ptr device);
		VulkanCmdPool::Ptr GetIdlePool(uint32_t famly_index);
	private:
		VulkanCmdPoolManager() = default;
		DISALLOW_COPY_AND_ASSIGN(VulkanCmdPoolManager);
		~VulkanCmdPoolManager() = default;
	private:
		using PoolList = Vector<VulkanCmdPool::Ptr>;
		PoolList						pools_;
		VulkanDevice::Ptr				device_;
		bool							is_inited_ = false;
		std::mutex						mutex_;
	};

}
