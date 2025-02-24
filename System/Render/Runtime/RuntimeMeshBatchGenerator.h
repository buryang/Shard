#pragma once
#include "Utils/CommonUtils.h"
#include "Scene/Scene.h"
#include "Graphics/HAL/HALResources.h"
#include "RuntimeMeshData.h"

namespace Shard::Render::Runtime
{
    /*mesh batch generate interface*/
    class DrawMeshBatchCollectorInterface
    {
    public:
        virtual void OnCollectBatchStream(MeshRangeStream& batch_range, MeshBatchStream& batch_stream) = 0;
        virtual ~DrawMeshBatchCollectorInterface() = default;
    };
}
