#ifndef _CULLING_INC_
#define _CULLING_INC_

#include "../CommonUtils.hlsli"
#include "../Math.hlsli"
#include "ClusterCommon.hlsli"

//64bit-atomic: 32-bit Depth; 32-bit Triangle/Visible Cluster ID
struct VisibleID
{
    uint64 packed_value;
};

bool IntersectSize(float4 clip_min, float4 clip_max)
{
    float2 rect = clip_max.xy - clip_min.xy;
    float2 clip_threshold = 111;//todo
    return any(rect > clip_threshold);
}

/*return hpos cull bits*/
uint GetCullBits(float4 hpos)
{
    uint cull_bits = 0;
    cull_bits |= hpos.x < -hpos.w ? 1 : 0;
    cull_bits |= hpos.x > hpos.w ? 2 : 0;
    cull_bits |= hpos.y < -hpos.w ? 4 : 0;
    cull_bits |= hpos.y > hpos.w ? 8 : 0;
    cull_bits |= hpos.z < 0 ? 16 : 0;
    cull_bits |= hpos.z > hpos.w ? 32 : 0;
    cull_bits |= hpos.w <= 0 ? 64 : 0;
    return cull_bits;
}

float4 GetBoxCorner(float3 bbox_min, float3 bbox_max, int n)
{
    bool3 use_max = bool3((n & 0x1) != 0, (n & 0x2) != 0, (n & 0x4) != 0);
    return float4(lerp(bbox_min, bbox_max, use_max), 1);
}

float4 GetClip(float4 hpos, out bool valid)
{
    const float epsilon = 1.2e-7f;
    valid = !(epsilon < hpos.w && hpos.w < epsilon);
    return float4(hpos.xyz / abs(hpos.w), hpos.w);
}

/*compare 8 corner vertices with frustum*/
bool IntersectFrustum(float3 bbox_low, float3 bbox_high, float4x4 transform, out float4 clip_min, out float4 clip_max, out bool is_clip_valid)
{
    float4 world_proj = /*viewlast.proj*/ * transform;
    bool valid;
    float4 hpos = world_proj * GetBoxCorner(bbox_low, bbox_high, 0);
    float4 clip = GetClip(hpos, valid);
    uint bits = GetCullBits(hpos);
    float4 clip_min_ = clip;
    float4 clip_max_ = clip;
    bool is_clip_valid_ = valid;
    
    UNROLL
    for (int n = 1; n < 8; ++n)
    {
        hpos = world_proj * GetBoxCorner(bbox_low, bbox_high, n);
        clip = GetClip(hpos, valid);
        bits &= GetCullBits(hpos);
        clip_min_ = min(clip_min_, clip);
        clip_max_ = max(clip_max_, clip);
        is_clip_valid_ = is_clip_valid_ && valid;
    }

    clip_min = float4(clamp(clip_min_.xy, -1, 1), clip_min_.zw);
    clip_max = float4(clamp(clip_max_.xy, -1, 1), clip_max_.zw);
    is_clip_valid = is_clip_valid_;
    return bits == 0;
}

bool IntersectHiz(float4 clip_min, float4 clip_max, float4 hiz_size_factors)
{
    clip_min.xy = clip_min.xy * 0.5 + 0.5;
    clip_max.xy = clip_max.xy * 0.5 + 0.5;
    
    clip_min.xy *= hiz_size_factors.xy;
    clip_max.xy *= hiz_size_factors.xy;
    
    clip_min.xy = min(clip_min.xy, hiz_size_factors.zw);
    clip_max.xy = min(clip_max.xy, hiz_size_factors.zw);
    
    vec2 size = clip_max.xy - clip_min.xy;
    float max_size = max(size.x, size.y) * hiz_size_max;
    float mip_level = ceil(log2(max_size));
    float depth = 
    
    return clip_min.z <= depth + xx;
}

struct TraversalClusterCullingCallback
{
    void ProcessCluster(Cluster cluster)
    {
    
    };
};

#endif //_CULLING_INC_ 