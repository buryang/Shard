#include "RHI/Vulkan/RHICommandVulkan.h"

namespace MetaInit::RHI {

	using namespace Vulkan;
	using CommandType = RHICommandPacketInterface::ECommandType;
	RHICommandContext::Ptr RHICommandContext::Create()
	{
		auto context = new RHICommandContextVulkan;
		return context;
	}

	Vulkan::RHICommandContextVulkan::RHICommandContextVulkan()
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
			//deal with error
		}
		++cmd_count_;
		return;
	}

	void Vulkan::RHICommandContextVulkan::SetStreamSource(uint32_t stream_index, uint32_t offset)
	{
		//todo 
	}

}