#include "Renderer/RtRenderBarrier.h"

namespace Shard::Renderer {

    void RtBarrierBatch::Submit(RHI::RHICommandContext& context)
    {
        //FIXME other work
        SmallVector<> barriers;
        if (nullptr != dependencies_) {
            //add barriers to end
        }
        context.SetBarrierBatch(*this);
    }
}