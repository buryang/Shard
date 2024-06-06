#include "Graphics/HAL/HALGlobalEntity.h"
#include "Graphics/HAL/HALCommandIR.h"
#include "Graphics/HAL/HALGlobalEntity.h"
#include "Graphics/Effect/DebugView/EffectDebugView.h"
#define _USE_MATH_DEFINES 
#include <math.h>

#if 1//def DEVELOP_DEBUG_TOOLS

template<typename T>
void* operator new(size_t size, Shard::SmallVector<T>& heap) noexcept
{
    assert(sizeof(T) == size);
    heap.emplace_back(T);
    return &heap.back();
}

namespace Shard::Effect
{
    using namespace System::DebugView;

    void DebugViewRender::Init()
    {
        auto pso_lib = HAL::HALGlobalEntity::Instance()->GetOrCreatePSOLibrary();
        assert(pso_lib != nullptr);
        HAL::HALPipelineStateObjectInitializer line_pso;
        {
            line_pso.gfx_.primitive_topology_ = EInputTopoType::eLineStrip;
            line_pso.gfx_.vertex_shader_ = nullptr;
            line_pso.gfx_.geometry_shader_ = nullptr;
            line_pso.gfx_.pixel_shader_ = nullptr; //todo
        }
        debug_pso_[LINE_TYPE] = pso_lib->GetOrCreatEPipeline(line_pso);
        HAL::HALPipelineStateObjectInitializer point_pso;
        {
            point_pso.gfx_.primitive_topology_ = EInputTopoType::ePointList;
        }
        debug_pso_[POINT_TYPE] = pso_lib->GetOrCreatEPipeline(point_pso);
    }

    void DebugViewRender::UnInit()
    {
        for (auto pso : debug_pso_) {
            pso = nullptr;
        }
        //nothing ??
    }

    void DebugViewRender::Render(DebugViewContext& ctx, HAL::HALCommandContext* cmd_buffer)
    {
        HAL::HALBeginRenderPassPacket begin_pass_end;
        begin_pass_end.render_target_ = xxx;
        begin_pass_end.roi_ = xxx;
        cmd_buffer->Enqueue(&begin_pass_end);

        if (!ctx.line_batcher_.empty())
        {
            DrawLines(ctx.line_batcher_, cmd_buffer);
        }

        if (!ctx.point_batcher_.empty())
        {
            DrawPoints(ctx.point_batcher_, cmd_buffer);
        }

        HAL::HALEndRenderPassPacket end_pass_end;
        cmd_buffer->Enqueue(&end_pass_end);

        ctx.Reset();
    }

    void DebugViewRender::DrawLines(const SmallVector<LineViewCommand>& lines, HAL::HALCommandContext* cmd_buffer)
    {
        //bind pso
        HAL::HALBindPSOPacket bind_cmd;
        bind_cmd.pso_ = debug_pso_[LINE_TYPE];
        bind_cmd.bind_point_ = HAL::HALBindPSOPacket::eGFX;
        cmd_buffer->Enqueue(&bind_cmd);
        //get gpu buffer
        
        //draw
        HAL::HALDrawIndexedIndirectPacket draw_cmd;
        cmd_buffer->Enqueue(&draw_cmd);
    }

    void DebugViewRender::DrawPoints(const SmallVector<PointViewCommand>& points, HAL::HALCommandContext* cmd_buffer)
    {
        //bind pso
        HAL::HALBindPSOPacket bind_cmd;
        bind_cmd.pso_ = debug_pso_[POINT_TYPE];
        bind_cmd.bind_point_ = HAL::HALBindPSOPacket::eGFX;
        cmd_buffer->Enqueue(&bind_cmd);

        //get gpu buffer

        //draw
        HAL::HALDrawIndexedIndirectPacket draw_cmd;
        cmd_buffer->Enqueue(&draw_cmd);
    }

    void EffectDrawDebugView::Init()
    {
        DebugViewRender::Init();
    }

    void EffectDrawDebugView::Unit()
    {
        DebugViewRender::UnInit();
    }

    void EffectDrawDebugView::Draw(Render::RenderGraphBuilder& builder, System::DebugView::DebugViewSystem& debugview)
    {
        DebugViewRender render;
        auto debug_ctx = debugview.GetDebugViewContext();
        builder.GetRenderGraph().AddPass([&]()
            auto graph_exe = builder.GetRenderGraphExe();
            assert(graph_exe.get() != nullptr);
            //auto cmd_buffer = graph_exe.
            render.Render(debug_ctx, cmd_buffer);
        }, "DebugView Render");
    }

}

#endif