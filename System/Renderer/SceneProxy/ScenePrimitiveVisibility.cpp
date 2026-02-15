#include "Utils/CommonUtils.h"
#include "Core/EngineGlobalParams.h"
#include "Scene/Primitive.h"
#include "ScenePrimitiveVisibility.h"


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
