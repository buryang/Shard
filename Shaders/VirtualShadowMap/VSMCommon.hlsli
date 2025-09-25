#ifndef _VSM_COMMON_INC_
#define _VSM_COMMON_INC_

#include "../CommonUtils.hlsli"
#include "../Math.hlsli"
#include "../VirtualTexture/VTPageCommon.hlsli"

#define VSM_LOG2_RECEIVER_SIZE 3u
#define VSM_RECEIVER_SIZE (1<<VSM_LOG2_RECEIVER_SIZE) //8x8 receiver 
#define VSM_RECEIVER_SIZE_MASK (VSM_RECEIVER_SIZE-1u) 
#define VSM_RECEIVER_SIZE_QUAD_MASK (VSM_RECEIVER_SIZE_MASK >> 1u) //mask in quad's cell

#define VSM_PROJ_MAX_SCALE (1.0f/15.f)

//projection flags
#define VSM_PROJ_FLAG_UNCACHED (1u << 1) //light is unached, should be rendered as dynamic
#define VSM_PROJ_FLAG_UNREFERENCED (1u << 2) //light is not rendered 
#define VSM_PROJ_FLAG_IS_FIRST_PERSION (1u << 3) //whether this is first persion SM
#define VSM_PROJ_FLAG_USE_RECEIVER_MASK (1u << 4) //whether this light use receiver mask to accelerate

#define VSM_MAX_SINGLE_PAGE_SHADOW_MAP 128u  //single page shadow map 

#define VSM_DEFAULT_THRAD_GROUP 64u
#define VSM_DEFAULT_THREAD_GROUP_X VSM_DEFAULT_THRAD_GROUP
#define VSM_DEFAULT_THREAD_GROUP_Y VSM_DEFAULT_THRAD_GROUP

#define VSM_READBACK_ENABLED 1

inline uint4 ConvertTexelRectToPageRect(uint4 texel_rect)
{
    return texel_rect >> VT_LOG2_PAGE_SIZE;
}

//divide receiver mask(same size as page) to 2x2 quad
//record each quad's mask in a single uint number
//get receiver local sub address from virtual address
inline uint2 GetReceiverMaskAddress(uint2 virtual_address)
{
    return (virtual_address >> (VT_LOG2_PAGE_SIZE - VSM_RECEIVER_SIZE)) & VSM_RECEIVER_SIZE_QUAD_MASK;
}

inline uint2 GetReceiverMaskQuad(uint2 virtual_address)
{
    return (virtual_address >> (VT_LOG2_PAGE_SIZE - 0x1u)) & 0x1u;
}

struct VSMProjectionSample
{
    float shadow_factor;
};

#define VSM_PAGE_MARK_BITS 4u
#define VSM_PAGE_MARK_SHIFT 28u
#define VSM_PAGE_MARK_MASK (1u << VSM_PAGE_MARK_BITS) - 1u
#define VSM_PAGE_FLAG_IS_DIRTY (1 << 0u)
#define VSM_PAGE_FLAG_IS_SINGLE_PAGE (1 << 1u)
#define VSM_PAGE_FLAG_IS_RESIDENT  (1 << 2u)

//special realization of virtual page for vsm
struct VSMVirtualPage 
{
    uint flags;
    uint instance_id;
    float4x4 world_to_shadow; //translated world to shadow?
    
    bool IsValid()
    {
        return (flags & VSM_PAGE_FLAG_IS_RESIDENT) && (flags & (VSM_PAGE_FLAG_IS_DIRTY | VSM_PAGE_FLAG_IS_SINGLE_PAGE));
    }
    bool IsSinglePage()
    {
        return flags & VSM_PAGE_FLAG_IS_SINGLE_PAGE;
    }
    bool IsResident()
    {
        return flags & VSM_PAGE_FLAG_IS_RESIDENT;
    }
    bool IsDirty()
    {
        return flags & VSM_PAGE_FLAG_IS_DIRTY;
    }
};

//vsm page draw command
struct VSMPerPageDrawCmd
{
    
};

/*global virtual shadow map bindless information
* the same design as gpu scene, with one global instance
* and one builder bind with this instance
*/
struct VSM
{
    uint light_data_bf_index; //light data buffer
    uint max_page_tbl_size; //max page tbl size for single shadow map
    uint page_tbl_tex2d_index; //all vsm's page tables share global entity 
    uint page_flags_tex2d_index;
    
    //page receiver mask
    uint page_receive_mask_tex2d_index;
#if VSM_READBACK_ENABLED
    uint vsm_stats_readback_bf_index; //read back buffer for cpu read
#endif
};

struct VSMBuilder
{
    uint light_data_bf_index;
    uint page_tbl_tex2d_index; //all vsm's page tables share global entity 
    uint page_draw_command_rw_buffer_index;
    uint page_mark_queue_rw_buffer_index;
#if VSM_READBACK_ENABLED
    uint vsm_build_stats_rw_buffer_index; //build stats buffer
#endif
};

inline uint GetVSMPageIndex(uint vsm_id, uint2 page_coord, uint page_tbl_width)
{
    return wsm_id * max_page_tbl_size + page_coord.y * page_tbl_width + page_coord.x;
}

//add blue noise to sample to break up banding 
inline void DitherProjectionSample(uint2 pixel, inout VSMProjectionSample result)
{
    if (result.shadow_factor_ < 1.0f && result.shadow_factor_ > VSM_PROJECTION_MAX_SCALE / 4.0f)
    {
        result.shadow_factor_ += (BlueNoise1D(result.shadow_factor_, view.frame_index_) - 0.5f) * VSM_PROJECTION_MAX_SCALE;
        result.shadow_factor_ = saturate(result.shadow_factor_);
    }
}

//calculate radius around the clipmap origin that clip index must cover
inline float ClipIndexToRadius(uint clip_index)
{
    return float(1 << (clip_index + 1));
}

inline float3 Clip0ToClipN(in float3 origin, in uint clip_index)
{
    float3 res = origin;
    res.xy *= float(1.0 / float(1 << clip_index));
    return res;
}

//return values on the range of [-1, 1]
inline float3 CalcRenderClipValueFromWorldPos(in float3 world_pos, in uint clip_index)
{
    
}

//return the clip index ndc inside
inline uint ClipIndexSelect(in float2 ndc)
{
    const float2 clip_index = float2(ceil(log2(max(abs(ndc), 1.f))));
    return uint(max(clip_index.x, clip_index.y));
}

//calc physX page offset(offset.x, offset.y, pool_index) from virtual coordinate
inline float3 CoordsVirtualToPhysX(in float2 ndc)
{
    
}

//todo toroidal addressing



#endif //_VSM_COMMON_INC_