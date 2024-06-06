#pragma once
#include "Utils/CommonUtils.h"
#include "Graphics/Render/RenderShader.h"
#include "Graphics/Render/RenderGraphBuilder.h"

namespace Shard::Effect
{
    class MINIT_API EffectDrawBase
    {
    public:
        static void Draw(Render::RenderGraphBuilder& builder);
    };
}