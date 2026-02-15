#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Memory.h"
#include "Utils/SimpleEntitySystemPrimitive.h"

namespace Shard::Renderer
{
    //global renderer allocator
    auto& GetRendererAllocator() {
        return *Utils::TLSScalablePoolAllocatorInstance<uint8_t, POOL_RENDER_SYSTEM_ID>();
    }

    template<typename T>
    using RenderArray = Vector<T, Allocator = Utils::ScalablePoolAllocator> ;

    using ECSRenderAbleTag = Utils::ECSTagComponent<struct RenderAble_>;

    //view matrix for current frame
    //todo unreal engine translated world proj matrix
    struct ECSViewRenderProjMatrix
    {
        //view matrix
        float4x4    view_matrix_;
        float4x4    inv_view_matrix_; 

        //translated camera position as world origin
        float4x4    translated_view_matrix_;
        float4x4    inv_translated_view_matrix_;

        float4x4    proj_matrix_;
        float4x4    inv_proj_matrix_;
    };

    //store view related temporal state
    struct ECSViewRenderPrevProjMatrix
    {
        float4x4    view_matrix_;
        float4x4    inv_view_matrix_;

        float4x4    translated_view_matrix_;
        float4x4    inv_translated_view_matrix_;
        //etc
    };

    //view render propertoes
    struct ECSViewRenderRelevant
    {
        //render frame index
        uint64_t    frame_index_;
        float       lod_dis_factor_{ 1.f }; //lod distance factor

        Utils::Entity   primary_view_entites_; //primiary view entity when it's a secondary view entity like stereo-viewo view

        //uint16_t is_fade_enabled_ : 1;
        //uint16_t is_primary_view_ : 1;
        //extra flags user define
        uint32_t    extra_flags_;
    };


    //a shadow related view
    using ECSViewShadowRenderTag = Utils::ECSTagComponent<struct ViewShadowRender>;

    struct ECSViewRenderResouces
    {
        Render::BufferHandle    view_uniform_buffer_;
        //fxr module shared render target
        Render::TextureHandle    render_target_;
    };

    //LOD selection view proxy, for stereo/VR need only using left view for LOD selection
    //to avoid inconsistent between views, default set to the view render itself
    using ECSViewLODProxy = Utils::Entity;

    struct ViewVisibleLightInfo
    {
        Utils::Entity   light_entity_;
        uint32_t    is_in_view_frustum_ : 1;
        uint32_t    is_in_draw_range_ : 1;
    };

    struct ECSViewVisibileLights
    {
        //visible lights( for example spot/point/texture light in view frustum)
        //directional lights are always visible
        RenderArray<ViewVisibleLightInfo>   visible_lights_;
    };

    struct ECSViewVisibilePrimitives
    {
        using ViewInstanceMask = BitVector<ViewLocalAllocator>;
        //view related instance visiblity mask
        ViewInstanceMask    instance_mask_;
    };

    //view related LOD temporal information
    struct ECSViewTemporalLODInfo
    {
        float3 temporal_view_orgin_[2];
        float temporal_last_time_[2];
        float temporal_lod_lag_; //
    };

    struct ECSInstanceRenderRelevant
    {
        uint16_t lod_index_{ ~0u };
        uint16_t is_translent_ : 1;
        uint16_t is_hair_ : 1;
    };

    //tag a entity must be despawned each frame
    using ECSInstanceTemporalTag = Utils::ECSTagComponent<struct TemporalRenderTag>;

    //light related render relevanr
    struct ECSLightRenderRelevant
    {
        float4  linear_color_;
        //whether it's moable light
        uint8_t is_movable_ : 1;
        //whether its position and parameters is static 
        uint8_t is_static_lighting_ : 1;
        //whether it has static direct shadowning
        uint8_t is_static_shadowing_ : 1;
        //whether casts dynamic shadows
        uint8_t is_dynamic_shadow_caster_ : 1;
        //whether casts static shadows
        uint8_t is_static_shadow_caster_ : 1;
        //whether light from this light transmits through surfaces with subsurface scattering profiles. Requires light to be movable
        uint8_t is_transmission_ : 1;
    };
    
}
