#ifndef _DDGI_INC_
#define _DDGI_INC_

#include "../CommonUtils.hlsli"
#include "../Math.hlsli"
#include "../ComputeShaderUtils.hlsli"

//realize spatial-hash according to https://github.com/NVIDIA-RTX/SHARC
//using spatial-hash to store all radiance probes

#define DDGI_HASH_GRID_POSITION_BITS 17u
#define DDGI_HASH_GRID_POSITION_MASK ((1u << DDGI_HASH_GRID_POSITION_BITS) - 1u)
#define DDGI_HASH_GRID_LEVEL_BITS 10u
#define DDGI_HASH_GRID_LEVEL_MASK ((1u << DDGI_HASH_GRID_LEVEL_BITS) - 1u)
#define DDGI_HASH_GRID_NORMAL_BITS 3u
#define DDGI_HASH_GRID_NORMAL_MASK ((1u << DDGI_HASH_GRID_NORMAL_BITS) - 1u)
#define DDGI_HASH_GRID_MAP_BUCKET_SIZE 16u
#define DDGI_HASH_GRID_INVALID_KEY 0u
#define DDGI_HASH_GRID_INVALID_CACHE_INDEX 0xffffffffu

//#define DDGI_HASH_GRID_USE_NORMALS 

#ifndef DDGI_HASH_GRID_POSITION_OFFSET
#define DDGI_HASH_GRID_POSITION_OFFSET           float3(0.0f, 0.0f, 0.0f)
#endif

#ifndef DDGI_HASH_GRID_POSITION_BIAS
#define DDGI_HASH_GRID_POSITION_BIAS             1e-4f   // may require adjustment for extreme scene scales
#endif

#ifndef DDGI_HASH_GRID_NORMAL_BIAS
#define DDGI_HASH_GRID_NORMAL_BIAS               1e-3f
#endif

#define HashGridIndex uint
#define HashGridKey uint64_t

struct HashGridConfig
{
    float3 camera_position;
    float logarithm_base;
    float scene_scale;
    float level_bias;
};

float HashGridLog(float value, float base)
{
    return log(value) / log(base);
}


uint HashGridHash(HashGridKey key)
{
    return HashJenkins32(uint(key) & 0xffffffffu) ^ HashJenkins32(uint((key >> 32u) & 0xffffffffu));
}


#ifndef DDGI_PROBE_SPACING
#define DDGI_PROBE_SPACEING 50.0f
#endif
#define DDGI_LEAPFROG_THRESHOLD (DDGI_PROBE_SPACEING*0.5f)

#ifndef DDGI_PROBE_COUNT
#define DDGI_PROBE_COUNT 1000u
#endif

#define DDGI_RAYS_PER_PROBE 256u
#define DDGI_ATLAS_SIZE_X 1024u
#define DDGI_ATLAS_TILE_SIZE 16u

#define DDGI_TRACKING_WIN_SIZE_X 16u
#define DDGI_TRACKING_WIN_SIZE_Y 16u 
#define DDGI_TRACKING_WIN_SIZE_Z 16u

//attenuation factor for blend region
#define DDGI_BLEND_REGION_ATTENUATION 1u 

/**
 * ray tracing fibonacci pattern
 */


struct DDGIConfig
{
    HashGridConfig  grid_config;
    float   radiance_scale;
    bool    antifilefly_filter_enabled;
};

enum EProbeState
{
    eOff，
    eSleeping,
    eNewlyAwake,
    eAwake,
    eNewlyVigilant,
    eVigilant,
};

struct Probe
{
    float3 position; //world position
    EProbeState state; 
    float hysterisis;

    void Init(float3 pos)
    {
        position = pos;
        state = EProbeState::eOff;
        hysterisis = 0.8f;
    }
};






#endif //_DDGI_INC_