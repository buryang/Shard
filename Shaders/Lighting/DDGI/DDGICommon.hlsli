#ifndef _DDGI_INC_
#define _DDGI_INC_

#include "../CommonUtils.hlsli"
#include "../Math.hlsli"
#include "../ComputeShaderUtils.hlsli"

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


//oc

#endif //_DDGI_INC_