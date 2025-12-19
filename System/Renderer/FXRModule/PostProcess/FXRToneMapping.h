#pragma once

#include "FXRPPCommon.h"

namespace Shard::Renderer::FXR
{

    class FXRToneMappingDraw final : public FXRDrawBase
    {
    public:
        DECLARE_RENDER_INDENTITY(FXRToneMappingDraw);
        struct ViewRenderState
        {
            //tonemapping LUT texture for all views
            RenderTexture::Handle tonemapping_LUT_;
        };
    }
}
