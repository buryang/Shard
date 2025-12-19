#include "Utils/CommonUtils.h"
#include "Graphics/FXRModule/DebugView/FXRImguiView.h"

#if defined(DEVELOP_DEBUG_TOOLS)&&defined(ENABLE_IMGUI)
namespace Shard::Renderer::FXR
{
    void FXRDrawImguiView::Init()
    {
    }

    void FXRDrawImguiView::Unit()
    {
    }

    void FXRDrawImguiView::Draw(Render::RenderGraph& graph, System::DebugView::DebugViewSystem& debugview)
    {
        graph.AddPass([](void) { 
            debugview.GetImGUIWrapper()->Render();
        });

        //todo move pipeline and command buffer logic from HAL here
    }
}
#endif