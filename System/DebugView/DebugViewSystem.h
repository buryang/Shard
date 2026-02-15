#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/SimpleEntitySystemPrimitive.h"

#if 1 //def DEVELOP_DEBUG_TOOLS

namespace Shard::Renderer::FXR
{
    class FXRDrawDebugView;
    class DebugViewRender;
}

namespace Shard::System::DebugView
{
    struct LineViewCommand
    {
        float3    start_pos_;
        float3    end_pos_;
        vec4    color_;
        float    thickness_;
        float    delta_time_;//?? esoterica
    };

    struct PointViewCommand
    {
        float3    pos_;
        vec4    color_;
        float    thickness_;
    };

    class MINIT_API DebugViewContext
    {
    public:
        DebugViewContext() = default;
        void DrawLine(float3 line_start, float3 line_end, vec4 color, float thickness);
        void DrawPoint(float3 position, vec4 color, float thickness);
        void DrawCircle(float3 center, uint32_t segments, vec4 quat, vec4 color, float radius, float thickness);
        void DrawBox(float3 center, float3 size, vec4 quat, vec4 color, float thickness);
        void DrawSphere(float3 center, uint32_t x_segments, uint32_t y_segments, vec4 color, float radius, float thickness);
        void DrawCapsule(vec4 color, float half_height, float radius, float thickness);
        void DrawCylinder(vec4 color, float half_height, float radius, float thickness);
        void DrawFrustum(const mat4 frustum_matrix, vec4 color, float thickness);
        void DrawAxis(float axis_length, float thickness);
        void Reset();
        DebugViewContext& operator+=(DebugViewContext&& rhs);
    private:
        friend class FXR::DebugViewRender;
        SmallVector<LineViewCommand>    line_batcher_;
        SmallVector<PointViewCommand>    point_batcher_;
    };

    class MINIT_API DebugViewSystem :public Utils::ECSSystem
    {
    public:
        void Init() override;
        void UnInit() override;
        void PreUpdate() override;
        void Update(ECSSystemUpdateContext& ctx) override;
        void MergeThreadContext(DebugViewContext&& thread_context);
#ifdef ENABLE_IMGUI
        FORCE_INLINE HAL::HALImGUILayer* GetImGUIWrapper() {
            return imgui_rhi_;
        }
#endif
    private:
        friend class Shard::Renderer::FXR::FXRDrawDebugView;
        FORCE_INLINE DebugViewContext& GetDebugViewContext() {
            return debug_context_;
        }
    private:
        DebugViewContext debug_context_;
#ifdef ENABLE_IMGUI
        HAL::HALImGUILayer*    imgui_rhi_{ nullptr };
#endif
    };
}
#endif