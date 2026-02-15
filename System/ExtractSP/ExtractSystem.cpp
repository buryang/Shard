#include "ExtractSystem.h"

namespace Shard::ExtractSP
{
    void ExtractRenderRelevantSystem::Init()
    {
        //regist global singleton event pending vector
    }

    void ExtractRenderRelevantSystem::Update(Utils::ECSSystemUpdateContext& ctx)
    {
        if (ctx.phase_ == EUpdatePhase::ePostPhysX) {
            EnumerateViewsToRender();
            BeginExtractJobs();

            FinalizeExtractJobs();
        }
    }

    bool ExtractRenderRelevantSystem::IsPhaseUpdateBefore(const ECSSystem& other, Utils::EUpdatePhase phase) const
    {
        if (other.) //check whether it's render system
        {
            return true;
        }

        return false;
    }

    bool ExtractRenderRelevantSystem::IsPhaseUpdateEnabled(Utils::EUpdatePhase phase) const
    {
        return phase == Utils::EUpdatePhase::ePostPhysX;
    }

    void ExtractRenderRelevantSystem::BeginExtractJobs()
    {
        for () {
            ExtractPerView();
        }
    }

    void ExtractRenderRelevantSystem::FinalizeExtractJobs()
    {
    }

    Coro<void> ExtractRenderRelevantSystem::ExtractPerView()
    {
        //1.collect visibility primitive for this view
        
        co_return;
    }
}
