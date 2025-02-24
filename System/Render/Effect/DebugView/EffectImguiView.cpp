#include "Utils/CommonUtils.h"
#include "Graphics/Effect/DebugView/EffectImguiView.h"

#if defined(DEVELOP_DEBUG_TOOLS)&&defined(ENABLE_IMGUI)
namespace Shard::Effect
{
    void EffectDrawImguiView::Init()
    {
    }

    void EffectDrawImguiView::Unit()
    {
    }

    void EffectDrawImguiView::Draw(Render::RenderGraph& graph, System::DebugView::DebugViewSystem& debugview)
    {
        graph.AddPass([](void) { 
            debugview.GetImGUIWrapper()->Render();
        });

        //todo move pipeline and command buffer logic from HAL here
    }
}
#endif