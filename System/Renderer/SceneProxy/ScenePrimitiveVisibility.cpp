#include "Utils/CommonUtils.h"
#include "Core/EngineGlobalParams.h"
#include "ScenePrimitiveVisibility.h"

//force mesh lod, default ~0u mean not force
REGIST_PARAM_TYPE(UINT, RENDER_LOD_FORCE_VAL, ~0u);
//engine global lod factor bias
REGIST_PARAM_TYPE(FLOAT, RENDER_LOD_FACTOR_BIAS, 1.f); 

namespace Shard::Renderer
{
    void VisiblePrimitiveConsumer::AddBatches(const VisiblePrimitivesCollector& collector)
    {
       //
    }

    void VisiblePrimitiveStreamingConsumer::AddBatches(const VisiblePrimitivesCollector& collector)
    {
        VisiblePrimitiveConsumer::AddBatches(collector);
        //other work here
    }

    void VisiblePrimitiveStreamingConsumer::Flush()
    {
    }

    class 
}
