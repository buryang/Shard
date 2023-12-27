#pragma once
#include "Utils/CommonUtils.h"
#include "Graphics/RHI/RHIResources.h"
#include "Graphics/Renderer/RtRenderShader.h"

#if 1//def defined(DEVELOP_DEBUG_TOOLS)&&defined(ENABLE_IMGUI)
namespace Shard::Effect
{
    class MINIT_API EffectDrawImguiView
    {
    public:
        static void Init();
        static void Unit();
        static void Draw(Renderer::RtRenderGraphBuilder& builder, System::DebugView::DebugViewSystem& debugview);
    };
}
#endif