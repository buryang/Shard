#pragma once

#if 1//def DEVELOP_DEBUG_TOOLS
#include "Utils/CommonUtils.h"
#include "System/DebugView/DebugViewSystem.h"
#include "Graphics/Render/RenderGraphBuilder.h"
#include "Graphics/HAL/HALShaderLibrary.h"
#include "Graphics/HAL/HALCommand.h"
#include "../Base/FXRBaseModule.h"

namespace Shard::Renderer::FXR
{
    class MINIT_API DebugViewRender
    {
    public:
        enum
        {
            LINE_TYPE = 0,
            POINT_TYPE = 1,
            MAX_DEBUG_PRIMIVE_TYPE = 2,
        };
        static void Init();
        static void UnInit();
        void Render(System::DebugView::DebugViewContext& ctx, HAL::HALCommandContext* cmd_buffer);
    private:
        void DrawLines(const SmallVector<System::DebugView::LineViewCommand>& lines, HAL::HALCommandContext* cmd_buffer);
        void DrawPoints(const SmallVector<System::DebugView::PointViewCommand>& points, HAL::HALCommandContext* cmd_buffer);
    private:
        static Array<HAL::HALPipelineStateObject*, MAX_DEBUG_PRIMIVE_TYPE>    debug_pso_;
    };

    class MINIT_API FXRDrawDebugView : public FXRDrawBase
    {
    public:
        DECLARE_RENDER_INDENTITY(FXRDrawDebugView);
        static void Init();
        static void Unit();
        static void Draw(Render::RenderGraphBuilder& builder, System::DebugView::DebugViewSystem& debugview);
    };
}

#endif
