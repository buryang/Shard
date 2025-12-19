#pragma once
#include "Utils/CommonUtils.h"
#include "Graphics/HAL/HALResources.h"
#include "Graphics/Render/RenderShader.h"
#include "../Base/FXRBaseModule.h"

#if 1//def defined(DEVELOP_DEBUG_TOOLS)&&defined(ENABLE_IMGUI)
namespace Shard::Renderer::FXR
{
    class MINIT_API FXRDrawImguiView : public FXRDrawBase
    {
    public:
        static void Init();
        static void Unit();
        static void Draw(Render::RenderGraphBuilder& builder, System::DebugView::DebugViewSystem& debugview);
    };
}
#endif