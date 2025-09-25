#include "../CommonUtils.hlsli"

/**
* screen space shadows by Graham Aldridge(bendstdio)
 */

#define SSS_THREAD_GROUP_SIZE_X 64
#define SSS_HARD_SHADOW_SAMPLES 4u //Number of initial shadow samples that will produce a hard shadow
#define SSS_FADE_OUT_SAMPLES 8u //Number of samples over which to fade out the hard shadow
#define SSS_SAMPLE_COUNT 60u //samples per pixel, determines overall cost, as this value controls the length of the shadow


struct SSSParameters
{
    float surface_thickness;
    float shadow_contrast; 
    float binlinear_threshold; 
    float4 light_coordinate;
    
    //
    bool use_precision_offset;
    bool use_early_out; 
    bool ignore_edge_pixels;

    //culling/early out
    float2 depth_bounds;

    float far_depth;
    float near_depth;

    //sampling data
    float2 inv_depth_texture_size; //1.0/texture size

    //resource 
    uint depth_tex2d_rd_index;
    uint sss_output_tex2d_rw_index;


    void Init()
    {
        surface_thickness = 0.005f;
        shadow_contrast = 4.0f;
        binlinear_threshold = 0.2f;
        depth_bounds = float2(0.0f, 1.0f);
    }
};


inline bool IsPixelEarlyOut(uint2 pixel_xy, float depth)
{
    //todo custom depth bounds test
   （void)pixel_xy; //fix compiler warning
    return depth >= depth_bounds.y || depth <= depth_bounds.x;
}

void CalcWaveExtent(uint3 groupID /*SV_GroupID*/, uint thread_index, out float2 delta_xy, out float2 pixel_xy, out float pixel_distance, out bool major_axis_x)
{

}

[numthreads(SSS_THREAD_GROUP_SIZE_X, 1, 1)]
void CSGenerateSSS(uint3 groupID : SV_GroupID, uint thread_index : SV_GroupIndex)
{

}
