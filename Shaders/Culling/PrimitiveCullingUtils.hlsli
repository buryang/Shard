#ifndef _PRIMITIVE_CULLING_UTILS_INC_
#define _PRIMITIVE_CULLING_UTILS_INC_

/**some helper functions for box/sphere
* shared by all culling related code
*/

struct BoundBoxSphere
{
    float3 center;
    float3 extent;
    float4 sphere;
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
    valid = !(M_EPSILON < hpos.w && hpos.w < M_EPSILON);
    return float4(hpos.xyz / abs(hpos.w), hpos.w);
}

/*compare sphere with frustum*/
bool IntersectFrustum(float4 sphere, )
{

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

bool TestDistance(BoundBoxSphere bounding_box)
{

}

bool TestDistanceConservative(BoundBoxSphere bounding_box, float min_distance_sq, float max_distance_sq)
{

}

bool TestFrustum(BoundBoxSphere bounding_box, )
{

}

bool TestScreenSize(BoundBoxSphere bounding_box, float min_screen_size, float max_screen_size)
{

}

bool TestHZB(BoundBoxSphere bounding_box)
{

}

//global plane for water planar reflect etc
bool TestPlane(BoundBoxSphere bounding_box, float4 plane)
{

}

#define TEST_CULLING_DISTANCE (1u << 0u)
#define TEST_CULLING_FRUSTUM (1u << 1u)
#define TEST_CULLING_SCREEN_SIZE (1u << 2u)
#define TEST_CULLING_HZB （1u << 3u)
//todo
#define TEST_CULLING_GLOBAL_PLANE (1u << 5u)

//box culling intermediate data context
struct BoxCullingContext
{
    BoundBoxSphere local_box;
    BoundBoxSphere transformed_box; //transformed world/translated world coordinate 
    
    float4x4 transform; //current frame transform
    float4x4 prev_transform; //prev transform

    //culling parameters
    float2 screen_size_min_max; //screen size threshold
    float2 distance_sq_min_max; //distance square min/max for distance culling
    uint flags; //flags 
#ifdef ViSUALIZATION_CULLING_RESULT
    uint result_mask;
#endif

    void Init(BoundBoxSphere local_box, float4x4 transform, float4x4 prev_transform, uint flags)
    {

    }

#ifdef ViSUALIZATION_CULLING_RESULT
    #define CALL_FUNC(func, flag_bit, bounding_box, ...) \
        is_visible = func(bounding_box, __VA_ARGS__); \
        result_mask |= (is_visible ? flag_bit : 0u);
#else
    #define CALL_FUNC(func, flag_bit, bounding_box, ...) is_visible = func(bounding_box, __VA_ARGS__);
#endif
    bool Test()
    {

    }
};

#endif //_PRIMITIVE_CULLING_UTILS_INC_
