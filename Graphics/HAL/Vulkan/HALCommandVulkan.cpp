#include "HAL/Vulkan/HALCommandVulkan.h"

namespace Shard::HAL::Vulkan {
 
    static inline VkPrimitiveTopology TransPrimitiveTopologyToVulkan(EPrimitiveTopology topo) {
        switch (topo) {
        case EPrimitiveTopology::ePointList:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case EPrimitiveTopology::eLineList:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case EPrimitiveTopology::eLineStrip:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case EPrimitiveTopology::eTriangleList:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case EPrimitiveTopology::eTriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        default:
            LOG(ERROR) << fmt::format("vulkan not support topology{}", topo);
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
    }

    HALCommandContextVulkan::HALCommandContextVulkan():HALCommandContext(EContextQueueType::eGFX)
    {

    }

    void HALCommandContextVulkan::Enqueue(const HALCommandPacket* cmd)
    {
        switch (cmd->type_)
        {
        case ECommandType::eBindStreamSource:
            auto ss_cmd = static_cast<const HALBindStreamSourcePacket*>(cmd);
            BindVertexInput(ss_cmd->first_stream_index_, ss_cmd->offset_, 0u);
            break;
        case ECommandType::eSetViewPoint:
            break;
        default:
            PLOG(ERROR) << "not supprted command type" << static_cast<uint32_t>(cmd->Type()) << std::endl;
        }
        ++cmd_count_;
        return;
    }

    void HALCommandContextVulkan::Submit(HALGlobalEntity* rhi)
    {
        VulkanQueue::EQueueType queue_type{ VulkanQueue::EQueueType::eAllIn };
        if (GET_PARAM_TYPE_VAL(BOOL, DEVICE_ASYNC_COMPUTE) && GetFlags() == EFlags::eAsyncCompute) {
            queue_type = VulkanQueue::EQueueType::eNonGFX;
        }
        auto queue_ptr = VulkanQueue::Create(queue_type);
        queue_ptr->Submit(rhi_handle_, wait_semaphore_, signal_semaphore_);
        LOG(INFO) << "processed " << cmd_count_ << " commands";
    }

    /*maybe vulkan c++ warper not good*/
    void HALCommandContextVulkan::WaitEvent(Span<HALEvent*>& events)
    {
        eastl::unique_ptr<VkEvent[]> raw_events{ new VkEvent[events.size()] };
        for (auto n = 0; n < events.size(); ++n) {
            raw_events[n] = dynamic_cast<HALEventVulkan*>(events[n])->GetHandle();
        }
        rhi_handle_->WaitEvenets(raw_events.get(), events.size());
    }

    void HALCommandContextVulkan::BindVertexInput(const HALVertexBuffer* stream_data, uint32_t offset, uint32_t stride)
    {
    }

    void HALCommandContextVulkan::BindIndexInput(const HALBuffer* index_buffer, uint32_t offset)
    {
    }

    void HALCommandContextVulkan::BindPipeline(HALPipelineStateObjectVulkan* pso)
    {
    }

    HALCommandQueueVulkan::HALCommandQueueVulkan(VulkanDevice::SharedPtr parent, uint32_t queue_index, float priority):HALCommandQueue(queue_index, priority), VulkanDeviceObject(parent)
    {
    }

}


