#pragma once
#include "RHI/RHICommand.h"
#include "RHI/Vulkan/API/VulkanCmdContext.h"

namespace MetaInit::RHI {
	namespace Vulkan {
		class RHICommandContextVulkan final : public RHICommandContext
		{
		public:
			RHICommandContextVulkan();
			void Enqueue(const RHICommandPacketInterface::Ptr cmd) override;
			void Submit() override;
		private:
			void SetStreamSource(uint32_t stream_index, uint32_t offset);
			void BeginRenderPass();
			void EndRenderPass();
			void PushEvent();
			void PopEvent();
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