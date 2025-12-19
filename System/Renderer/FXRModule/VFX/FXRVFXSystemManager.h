#pragma once

#include "../Base/FXRBaseModule.h"
#include "../../PhyX/VFX/VFXSystem.h"

//todo 
//1. use two buffer ping/pong fro each vfx system, one for simulation, one for rendering
//2. does every vfx systen own its own data buffer? or share a big buffer?
//3. how to handle different data structure for different vfx system?
//4. so all work in done on GPU, does we need a readback mechanism for cpu side logic?


namespace Shard::Renderer::FXR
{
    //class FXRVFXSystemDelegate; 
    class FXRVFXSystemProxyManager : public FXR::FXRDrawBase
    {
    public:
        void Draw(Render::RenderGraph& graph, Span<ViewRender>& views, ERenderPhase phase) override;
    protected:
        //iterate all gpu simulation vfx and do simulations
        void SolveGPUSystemSimulation(Render::RenderGraph& graph); // solve all vfx system gpu simulation tasks
        void RenderAllSystems(Render::RenderGraph& graph, Span<ViewRender>& views);
    };
}
