#ifndef _DITHER_TRANSION_INC_
#define _DITHER_TRANSION_INC_

#include "../Platform.hlsli"

#if SM_MAJOR >= 6
#define DITHER_BLUE_NOISE 1
#elif SM)MAJOR >= 5
#define DITHER_RAND_HASH 1
#else 
#define DITHER_ORDERED 1
#endif

/**************************dither transition help functions(fade-in/out or LOD transition)*****************************************/
#if DITHER_BLUE_NOISE

#include "../BindlessResource.hlsli"

//blue-noise dithering(void-and_cluster) for high-end and animated fade
half GetDitheredTransitionFactor(float2 pixel_xy/*SV_POSITION*/, float dithered_transition_factor)
{
    //todo blue-noise texture sample
    Texture2D<float4> blue_noise = GetBindlessTexture(BINDLESS_TEXTURE_BLUE_NOISE);
    float2 uv = pixel_xy % float2(128.f); // Assuming 128 is the texture size
    float rand_val = blue_noise.Load(uv, 0).r;
    return (dithered_transition_factor < 0.f) ? (dithered_transition_factor + 1.f > rand_val) : (dithered_transition_factor < rand_val);
}

#elif DITHER_RAND_HASH
//unreal engine function, white noise-like spatialrandom hash(Noisy, organic, "film grain" or chaotic dissolve, very spread
half GetDitheredTransitionFactor(float2 pixel_xy/*SV_POSITION*/, float dithered_transition_factor)
{
    float rand_cos = cos(dot(floor(pixel_xy), float2(347.83451793f,3343.28371963f)));
    float rand_val = frac(rand_cos * 1000.f);
    half ret_val = (dithered_transition_factor < 0.f) ? (dithered_transition_factor + 1.f > rand_val) : (dithered_transition_factor < rand_val);
    return (ret_val - 0.001f);
}

half GetDitheredTransitionFactor(float2 pixel_xy/*SV_POSITION*/, uint frame_index, float dithered_transition_factor)
{
    float rand_cos = cos(dot(float3(pixel_xy, frame_index), float3(347.83451793f,3343.28371963f, 1234.56789f)));
    float rand_val = frac(rand_cos * 1000.f);
    half ret_val = (dithered_transition_factor < 0.f) ? (dithered_transition_factor + 1.f > rand_val) : (dithered_transition_factor < rand_val);
    return (ret_val - 0.001f);
}

#else 
//ordered dithering (Bayer matrix / threshold map), tiled pattern. read https://medium.com/the-bkpt/dithered-shading-tutorial-29f57d06ac39
half GetDitheredTransitionFactor(float2 pixel_xy/*SV_POSITION*/, float dithered_transition_factor)
{
    //standard 4x4 ordered dithering pattern, from https://en.wikipedia.org/wiki/Ordered_dithering#Threshold_map
    //packed 4x4 matrix to vec4, one byte stand for one matrix element
    const uint4 dither_thresholds = uint4(0x08020A0C, 0x0C040E06, 0x0B03090F, 0x0D07050F);
    const uint quant_dithered_transition_factor = int(round(dithered_transition_factor * 16.0f)); //scale to -16~16 range

    if(quant_dithered_transition_factor == 0u )
        return 1.f;
    //force uint to check abs(x) >= 16
    BRANCH
    if(uint(quant_dithered_transition_factor) >= 16u)
        return 0.f;
    const uint2 dither_pos = uint2(pixel_xy) & 3u; //mod 4
    const int threshold = int((dither_thresholds[dither_pos.y] >> (dither_pos.x << 3u)) & 0xfu); 

    if((quant_dithered_transition_factor > 0u && quant_dithered_transition_factor + threshold >= 16u) ||
        (quant_dithered_transition_factor < 0u && quant_dithered_transition_factor + threshold <= 0u))
        return 0.f;

    return 1.f;
}
#endif

//calculate dither level for each instance, then using this factor to calc pixel factor 
float GetVisibilityRangeDitheredFactor(float4 lod_range, float distance)
{
    // This encodes the following mapping:
    //
    //     `lod_range.`          x        y        z        w         distance
    //                   ←───────┼────────┼────────┼────────┼────────→
    //     Dither Level  -1     -1        0        0        1      1  Dither Level
    float offset = select(-1.f, 0.f, distance >= lod_range.z);
    float2 bounds = select(lod_range.xy, lod_range.zw, distance >= lod_range.z);
    float level = saturate((distance - bounds.x) / (bounds.y - bounds.x));
    return offset + level;
}


//https://github.com/runevision/Dither3D 3D dither?

#endif //_DITHER_TRANSITION_INC_