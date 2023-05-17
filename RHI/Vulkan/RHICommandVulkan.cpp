#include "RHI/Vulkan/RHICommandVulkan.h"

namespace MetaInit::RHI {

	using namespace Vulkan;
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

}