#pragma once
#include "RHI/RHICommand.h"
#include "RHI/Vulkan/API/VulkanCmdContext.h"
#include "RHI/Vulkan/RHIShaderVulkan.h"

namespace MetaInit::RHI {
	namespace Vulkan {
		class RHICommandContextVulkan final : public RHICommandContext
		{
		public:
			RHICommandContextVulkan();
			void Enqueue(const RHICommandPacketInterface::Ptr cmd) override;
			void Submit(RHIGlobalEntity::Ptr rhi) override;
			FORCE_INLINE VulkanCmdBuffer::SharedPtr Get() const {
				return rhi_handle_;
			}
		private:
			void BindVertexInput(const RHIVertexBuffer::Ptr stream_data, uint32_t offset, uint32_t stride);
			void BindIndexInput(const RHIBuffer::Ptr index_buffer, uint32_t offset);
			void BindPipeline(RHIPipelineStateObjectVulkan::Ptr pso);
			void BeginRenderPass();
			void EndRenderPass();
			void SignalEvent(RHIEvent::Ptr event);
			void WaitEvent(Span<RHIEvent::Ptr>& events);
			void CopyTexture();
			void CopyBuffer();
		private:
			VulkanCmdBuffer::SharedPtr rhi_handle_{ nullptr };
			SmallVector<VkSemaphore> wait_semaphore_;
			SmallVector<VkSemaphore> signal_semaphore_;
			uint32_t cmd_count_{ 0 };
		};
	}
}