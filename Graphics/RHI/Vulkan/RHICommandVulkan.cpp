#include "RHI/Vulkan/RHICommandVulkan.h"

namespace Shard::RHI::Vulkan {
    using CommandType = RHICommandPacketInterface::ECommandType;

    RHICommandContextVulkan::RHICommandContextVulkan():RHICommandContext(EFlags::eGraphics)
    {

    }

    void RHICommandContextVulkan::Enqueue(const RHICommandPacketInterface::Ptr cmd)
    {
        switch (cmd->Type())
        {
        case CommandType::eSetStreamSource:
            auto ss_cmd = static_cast<RHISetStreamSroucePacket*>(cmd);
            SetStreamSource(ss_cmd->stream_index_, ss_cmd->offset_);
            break;
        case CommandType::eSetViewPoint:
            break;
        default:
            PLOG(ERROR) << "not supprted command type" << static_cast<uint32_t>(cmd->Type()) << std::endl;
        }
        ++cmd_count_;
        return;
    }

    void RHICommandContextVulkan::Submit()
    {
        VulkanQueue::EQueueType queue_type{ VulkanQueue::EQueueType::eAllIn };
        if (GET_PARAM_TYPE_VAL(BOOL, DEVICE_ASYNC_COMPUTE) && GetFlags() == EFlags::eAsyncCompute) {
            queue_type = VulkanQueue::EQueueType::eNonGFX;
        }
        auto queue_ptr = VulkanQueue::Create(queue_type);
        queue_ptr->Submit(rhi_handle_, wait_semaphore_, signal_semaphore_);
        LOG(INFO) << "processed " << cmd_count_ << " commands";
    }

    void RHICommandContextVulkan::SetStreamSource(uint32_t stream_index, uint32_t offset)
    {
        //todo 
    }

    void RHICommandContextVulkan::SignalEvent(RHIEvent::Ptr event)
    {
        rhi_handle_->SignalEvent(dynamic_cast<RHIEventVulkan*>(event)->GetHandle());
    }

    /*maybe vulkan c++ warper not good*/
    void RHICommandContextVulkan::WaitEvent(Span<RHIEvent::Ptr>& events)
    {
        eastl::unique_ptr<VkEvent[]> raw_events{ new VkEvent[events.size()] };
        for (auto n = 0; n < events.size(); ++n) {
            raw_events[n] = dynamic_cast<RHIEventVulkan*>(events[n])->GetHandle();
        }
        rhi_handle_->WaitEvenets(raw_events.get(), events.size());
    }

}


