#pragma once
#include "Utils/CommonUtils.h"
#include "VulkanRHI.h"
#include <unordered_map>

namespace MetaInit
{
	VkCommandBufferAllocateInfo MakeCommandBufferAllocateInfo(VkCommandPool pool, VkCommandBufferLevel level, uint32_t buffer_count);
	VkCommandBufferBeginInfo MakeCommandBufferBeginInfo(VkCommandBufferUsageFlags flags);
	VkCommandPoolCreateInfo MakeCommandPoolCreateInfo(VkCommandPoolCreateFlags flags, uint32_t family_index);
	
	class VulkanRayTraceBindTable;
	class VulkanCmdPool
	{
	public:
		VulkanCmdPool() = default;
		DISALLOW_COPY_AND_ASSIGN(VulkanCmdPool);
		void Submit(VkQueue queue);
		void SubmitAsync();
		virtual ~VulkanCmdPool();
	private:
		VulkanDevice::Ptr			device_;
		VkCommandPool				pool_{ VK_NULL_HANDLE };
		Vector<VkCommandBuffer>		buffers_;
	};

	class VulkanRenderPass;
	class VulkanCmdBuffer
	{
	public:
		using Ptr = std::unique_ptr<VulkanCmdBuffer>;
		static Ptr Create(const VkCommandBufferAllocateInfo& cmd_info, VulkanCmdPool& cmd_pool);
		~VulkanCmdBuffer();
		VkCommandBuffer Get() {
			return buffer_;
		}
		/*cmd buffer type*/
		enum class Type :uint8_t
		{
			PRIMARY,
			SECONDARY,
			COUNT
		};
		/*cmd buffer state*/
		enum class State:uint32_t
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
	private:
		friend class VulkanCmdPool;
		VkCommandBuffer		buffer_{ VK_NULL_HANDLE };
		State				state_{ State::INITIAL };
		Type				type_{ Type::PRIMARY };
	};


}
