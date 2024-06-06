#pragma once
#include "HAL/HALCommand.h"
#include "HAL/Vulkan/API/VulkanCmdContext.h"
#include "HAL/Vulkan/HALShaderVulkan.h"

namespace Shard::HAL {
    namespace Vulkan {
        class HALCommandContextVulkan final : public HALCommandContext
        {
        public:
            HALCommandContextVulkan();
            void Enqueue(const HALCommandPacket* cmd) override;
            void Submit(HALGlobalEntity* rhi) override;
            FORCE_INLINE VulkanCmdBuffer::SharedPtr Get() const {
                return rhi_handle_;
            }
        private:
            void BindVertexInput(const HALVertexBuffer* stream_data, uint32_t offset, uint32_t stride);
            void BindIndexInput(const HALBuffer* index_buffer, uint32_t offset);
            void BindPipeline(HALPipelineStateObjectVulkan* pso);
            void BeginRenderPass();
            void EndRenderPass();
            void Barrier(HALTransitionInfo* trans);
            void BeginSplitBarrier(HALTransitionInfo* trans);
            void EndSplitBarrier(HALTransitionInfo* trans);
            void CopyTexture();
            void CopyBuffer();
        private:
            VulkanCmdBuffer::SharedPtr  rhi_handle_{ nullptr };
            SmallVector<VkSemaphore>    wait_semaphore_;
            SmallVector<VkSemaphore>    signal_semaphore_;
            /*use rhi transition_info ptr to index event*/
            Map<uint32_t, VkEvent>  events_;
            uint32_t cmd_count_{ 0 };
        };

        class HALCommandQueueVulkan final : public HALCommandQueue, public VulkanDeviceObject
        {
        public:
            explicit HALCommandQueueVulkan(VulkanDevice::SharedPtr parent, uint32_t queue_index, float priority=0.5f);
            void Submit(const HALCommandContext* command) override;
            void Submit(Span<const HALCommandContext*> commands) override;
            uint64_t GetTimeStampFrequency() override;
            void WaitFence() override;
            void SignalFence() override;
        private:
            DISALLOW_COPY_AND_ASSIGN(HALCommandQueueVulkan);
        };
    }
}