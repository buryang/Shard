#include "FXRPrepassAndVelocityRender.h"

namespace Shard::Renderer::FXR
{
    void FXRPrepassAndVelocityRender::Draw(Render::RenderGraph& builder, Span<ViewRender>& views, ERenderPhase phase)
    {
        //build velocity/depth prepass here
        //1. create velocity/depth texture
        //2. for each view, render velocity/depth
    }
}
