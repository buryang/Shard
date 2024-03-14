#include "Render/RenderBarrier.h"

namespace Shard::Render {

    void BarrierBatch::Submit(RHI::RHICommandContext& context)
    {
        //FIXME other work
        SmallVector<> barriers;
        if (nullptr != dependencies_) {
            //add barriers to end
        }
        context.SetBarrierBatch(*this);
    }
}