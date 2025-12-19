#pragma once

#include "Utils/CommonUtils.h"
#include "Utils/Math.h"
#include "../../RenderCommon.h"
#include "../Base/FXRBaseModule.h"

namespace Shard::Renderer::FXR
{

    //default halton max frames
    static constexpr auto halton_max_frames = 128u;

    /*low discrepancy progressive sequence
     * no clustering in either space or time
     */
    struct TAAHaltonLUT
    {
        RenderArray<float2> jitters_;

        void Init(uint32_t max_frames = halton_max_frames)
        {
            jitters_.resize(max_frames);
            for (auto i = 0u; i < max_frames; ++i)
            {
                auto hx = Utils::Halton(i + 1, 2u);
                auto hy = Utils::Halton(i + 1, 3u);
                jitters_[i] = float2(hx, hy);
            }
        }

        /* according to unreal engine slides : https://de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf
        *  halton(2, 3) fraction shared by all views 
        */
        float2 operator()(uint32_t frame_index, uint2 view_rect) const
        {
            const auto index = frame_index % jitters_.size();
            const auto jitterXY = jitters_[index] - 0.5f; // uniformly distribute in [-0.5, 0.5]
            return float2(jitterXY / float2(view_rect));
        }
    };


    class FXRTAADraw : public FXRDrawBase
    {
    public:
        struct ViewRenderState
        {
            uint32_t taa_frame_index_;
            uint32_t output_taa_frame_index_;
        };
        void PreDraw(Render::RenderGraphBuilder& builder, Span<ViewRender>& views) override;
        void Draw(Render::RenderGraphBuilder& builder, Span<ViewRender>& views) override;
        void PostDraw(Render::RenderGraphBuilder& builder, Span<ViewRender>& views) override;
    protected:
        DECLARE_RENDER_STATE_OP(FXRTAADraw);
    protected:
        uint32_t    taa_frame_length_{ 0u }; //temporal anti-filter frame length
    };
}
