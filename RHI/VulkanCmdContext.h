#pragma once
#include "Utils/CommonUtils.h"
#include "VulkanRHI.h"
#include <unordered_map>
#include <mutex>

namespace MetaInit
{
	VkCommandBufferAllocateInfo MakeCommandBufferAllocateInfo(VkCommandPool pool, VkCommandBufferLevel level, uint32_t buffer_count);
	VkCommandBufferBeginInfo MakeCommandBufferBeginInfo(VkCommandBufferUsageFlags flags);
	VkCommandPoolCreateInfo MakeCommandPoolCreateInfo(VkCommandPoolCreateFlags flags, uint32_t family_index);
	
	class VulkanRayTraceBindTable;
	class VulkanCmdPool
	{
	public:
		using Ptr = std::shared_ptr<VulkanCmdPool>;
		explicit VulkanCmdPool(VulkanDevice::Ptr device, VkCommandPoolCreateFlags flags, uint32_t family_index);
		DISALLOW_COPY_AND_ASSIGN(VulkanCmdPool);
		VkCommandPool Get() { return pool_; }
		VulkanDevice::Ptr GetDevice() { return device_; }
		void Submit(VkQueue queue);
		void SubmitAsync();
		void Reset();
		virtual ~VulkanCmdPool();
	private:
		void CreateBuffers();
	private:
		VulkanDevice::Ptr			device_;
		VkCommandPool				pool_{ VK_NULL_HANDLE };
		Vector<VkCommandBuffer>		buffers_;
		std::mutex					mutex_;
	};

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
			PRIMARY,
			SECONDARY,
			COUNT
		};
		/*cmd buffer state*/
		enum class EState:uint32_t
		{
			INITIAL,
			RECORDING,
			EXECUTABLE,
			PENDING,
			INVALID,
			COUNT
		};
		void SetViewPoint();
		void SetSwapChain();
		void SetLight();
		void Begin();
		void End();
		void BeginPass(VulkanRenderPass& render_pass);
		void EndPass(VulkanRenderPass& render_pass);
		void Dispatch();
		void Draw(uint32_t first_instance, uint32_t instance_count, uint32_t first_vertex, uint32_t vertex_count);		
		void TraceRay(std::unordered_map<uint32_t, VulkanRayTraceBindTable>& ray_binds, const glm::uvec3& dims);
		template<typename DataHandle>
		void Copy(DataHandle dst, DataHandle src);
		void Submit(VkQueue& queue);
		void Execute(Vector<VulkanCmdBuffer>& cmd_buffers);
		void Reset();
		VkCommandBuffer Get() { return handle_; }
	private:
		DISALLOW_COPY_AND_ASSIGN(VulkanCmdBuffer);
		explicit VulkanCmdBuffer(VkCommandBuffer cmd_buffer, VulkanCmdPool::Ptr cmd_pool);
	private:
		friend class VulkanCmdPool;
		VkCommandBuffer		handle_{ VK_NULL_HANDLE };
		VkFence				fence_{ VK_NULL_HANDLE };
		VulkanCmdPool::Ptr  pool_;
		EState				state_{ EState::INITIAL };
		EType				type_{ EType::PRIMARY };
	};


}
