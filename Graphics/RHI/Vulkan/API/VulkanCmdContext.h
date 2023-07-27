#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/Vulkan/API/VulkanRHI.h"
#include "RHI/Vulkan/API/VulkanPrimitive.h"
#include "RHI/Vulkan/RHISyncVulkan.h"
#include <mutex>

namespace Shard
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
		enum EState :uint8_t
		{
			eUsing,
			eIdle,
		};
		using SharedPtr = std::shared_ptr<VulkanCmdPool>;
		using Handle = VkCommandPool;
		explicit VulkanCmdPool(VkCommandPoolCreateFlags flags, uint32_t family_index);
		Handle Get() { return handle_; }
		EState GetState()const { return state_; }
		VkCommandPoolCreateFlags GetFlags() const { return flags_; }
		uint32_t GetFamilyIndex() const { return family_index_; }
		void SetState(EState state) { state_ = state; } 
		bool IsFull()const;
		void Reset();
		virtual ~VulkanCmdPool();
	private:
		DISALLOW_COPY_AND_ASSIGN(VulkanCmdPool);
	private:
		VkCommandPoolCreateFlags	flags_;
		uint32_t					family_index_;
		VkCommandPool				handle_{ VK_NULL_HANDLE };
		EState						state_{ EState::eIdle };
	};

	class VulkanRenderPass;
	class VulkanBuffer;
	class VulkanImage;
	class VulkanCmdBuffer
	{
	public:
		using SharedPtr = eastl::shared_ptr<VulkanCmdBuffer>;
		using Handle = VkCommandBuffer;
		static SharedPtr Create(VulkanCmdPool::SharedPtr cmd_pool);
		~VulkanCmdBuffer();
		/*cmd buffer type*/
		enum class EType :uint8_t
		{
			ePrimary,
			eSecondary,
			eNum
		};
		/*cmd buffer state*/
		enum class EState:uint32_t
		{
			eInitial,
			eRecording,
			eExecutable,
			ePending,
			eInvalid,
			eNum
		};
		FORCE_INLINE void SetState(EState state) {
			state_ = state;
		}
		FORCE_INLINE EState GetState()const {
			return state_;
		}
		FORCE_INLINE Handle Get() {
			return handle_;
		}
		FORCE_INLINE VulkanCmdPool::SharedPtr GetParent() {
			return parent_;
		}
		void Begin();
		void End();
		//compute pipeline must be bound to a command buffer before any dispatch commands are recorded
		void Dispatch(vec3 group_size);
		void DispatchIndirect(VulkanBuffer& buffer, const VkDeviceSize offset);
		void Draw(uint32_t first_instance, uint32_t instance_count, uint32_t first_vertex, uint32_t vertex_count);
		void DrawIndirect(const  VulkanBuffer& buffer, uint32_t offset, uint32_t draw_count, uint32_t stride);
		void DrawIndexed(uint32_t first_instance, uint32_t instance_count, uint32_t vertex_offset, uint32_t first_index, uint32_t index_count);
		void DrawIndexedIndirect(const  VulkanBuffer& buffer, uint32_t offset, uint32_t draw_count, uint32_t stride);
		void TraceRay(Map<uint32_t, VulkanRayTraceBindTable>& ray_binds, const glm::uvec3& dims);
		void Copy(VulkanBuffer& dst, uint32_t dst_offset,  VulkanBuffer& src, uint32_t src_offset, uint32_t size);
		void Copy(VulkanImage& dst,  VulkanBuffer& src);
		void Resolve(VulkanImage& dst,  VulkanImage& src);
		void Execute(Vector<VulkanCmdBuffer>& cmd_buffers);
		void Reset();
		void Barrier(VulkanImage& image, EResourceState new_state);
		void Barrier(VulkanBuffer& buffer, EResourceState new_state);
		void Barrier(const RHI::VulkanTransitionInfo& barrier_info);
		void PushConstants(const uint32_t flags, const uint32_t offset, const Span<uint8_t>& constants);
		void SignalEvent(VkEvent event);
		void WaitEvenets(VkEvent* events, uint32_t count);
		void BeginRenderPass();
		void EndRenderPass();
		void UpdateTopLevel();
		void UPdateBottomLevel();
		void UpdateShaderTable();
		void RayTrace();
	private:
		DISALLOW_COPY_AND_ASSIGN(VulkanCmdBuffer);
		explicit VulkanCmdBuffer(VulkanCmdPool::SharedPtr cmd_pool);
	private:
		friend class VulkanCmdPool;
		friend class VulkanQueue;
		VkCommandBuffer	handle_{ VK_NULL_HANDLE };
		VulkanCmdPool::SharedPtr  parent_;
		VkFence	fence_{ VK_NULL_HANDLE };
		VkFlags	flags_{ 0 };
		EState	state_{ EState::eInitial };
		EType	type_{ EType::ePrimary };
	};

	class VulkanCmdPoolManager
	{
	public:
		VulkanCmdPoolManager& Instance();
		VulkanCmdPool::SharedPtr GetIdlePool(VkCommandPoolCreateFlags flags, uint32_t famly_index);
	private:
		VulkanCmdPoolManager() = default;
		DISALLOW_COPY_AND_ASSIGN(VulkanCmdPoolManager);
	private:
		Map<uint64_t, SmallVector<VulkanCmdPool::SharedPtr>> pools_;
		std::mutex	mutex_;
	};

}
