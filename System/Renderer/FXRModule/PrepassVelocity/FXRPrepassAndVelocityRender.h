#pragma once
#include "Graphics/HAL/HALResources.h"
#include "../Base/FXRBaseModule.h"

namespace Shard::Renderer::FXR
{
    class FXRPrepassAndVelocityRender : public FXRDrawBase
    {
    public:
        FXRPrepassAndVelocityRender() = default; //todo
        void Draw(Render::RenderGraph& builder, Span<ViewRender>& views, ERenderPhase phase) override;
        //HAL::HALTexture& GetVelocityTexture() const { return velocity_texture_; }
        //HAL::HALTexture& GetDepthTexture() const { return depth_texture_; }
    protected:
        bool IsVelocityEnabled() const;
    protected:
        HAL::HALTexture velocity_texture_; //transient texture?? if this is transient texture, do not store it here
        HAL::HALTexture depth_texture_;
        //todo regist resources to render graph
        //place resource to global scene textures or here?

    };
}
