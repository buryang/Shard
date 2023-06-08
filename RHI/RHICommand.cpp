#include "RHI/RHICommand.h"

namespace MetaInit::RHI {

	void RHIVirtualCommandContext::Enqueue(const RHICommandPacketInterface::Ptr cmd)
	{
		record_cmds_.emplace_back(cmd);
	}

	void RHIVirtualCommandContext::Submit(RHIGlobalEntity::Ptr rhi)
	{
		LOG(ERROR) << "virtual command not submit, just record cmds";
	}

	void RHIVirtualCommandContext::ReplayByOther(RHICommandContext::Ptr other_context)
	{
		assert(this != other_context);
		for (auto cmd : record_cmds_) {
			other_context->Enqueue(cmd);
		}
	}

}