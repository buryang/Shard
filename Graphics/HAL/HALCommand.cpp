#include "HAL/HALCommand.h"

namespace Shard::HAL {

    void HALVirtualCommandContext::Enqueue(const HALCommandPacket* cmd)
    {
        record_cmds_.emplace_back(cmd);
    }

    void HALVirtualCommandContext::Submit(HALGlobalEntity* rhi)
    {
        LOG(INFO) << "virtual command not submit, just record cmds";
    }

    void HALVirtualCommandContext::ReplayByOther(HALCommandContext* other_context)
    {
        assert(this != other_context);
        for (auto cmd : record_cmds_) {
            other_context->Enqueue(cmd);
        }
    }

    void HALCommandContext::Enqueue(Span<const HALCommandPacket*> cmds)
    {
        for (auto command : cmds) {
            Enqueue(command);
        }
    }

}