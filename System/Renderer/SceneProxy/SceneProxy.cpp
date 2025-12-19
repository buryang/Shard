
#include "SceneProxy.h"

namespace Shard::Renderer {

    JobHandle SceneProxy::SyncWorldUpdates()
    {
        return JobHandle();
    }

    Utils::Coro<> SceneProxy::AsyncGatherPrimitiveUpdatesFromWorld()
    {
        co_return;
    }

    Utils::Coro<> SceneProxy::AsyncGatherLightUpdatesFromWorld()
    {
        co_return;
    }
}
