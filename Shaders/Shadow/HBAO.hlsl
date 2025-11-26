// Line-sweep(Line-Sweep Ambient Obscurance)/ horizon-based ambient obscurance (HBAO) pixel shader.
// Usage: bind depth and world/view normals. Tweak parameters in the constant buffer.

#include "../CommonUtils.hlsli"
#include "../BindlessResources.hlsli"

cbuffer AOParams : register(b0)
{
    float4x4 invprojection; // inverse projection matrix (clip->view)
    float2   invscreen;     // 1.0 / float2(screenWidth, screenHeight)
    float    radiuspx;      // maximum sweep radius in pixels
    float    intensity;     // final AO multiplier

    float    bias;          // small bias to avoid self-occlusion (in view-space units)
    int      numrays;       // number of sweep directions (e.g. 8)
    int      numsteps;      // samples per ray (e.g. 16)
    float    falloff;       // distance falloff exponent (>=1)

    float    normalbias;    // how much normal attenuates occlusion
    float2   noisescale;    // scale for random rotation (noise texture tiling)

    // bindless descriptor heap indices (defined/consumed by BindlessResources.hlsl)
    uint     depthindex;    // index for depth Texture2D in bindless heap
    uint     normalindex;   // index for normal Texture2D (optional) in bindless heap
    uint     randindex;     // index for noise Texture2D in bindless heap
    uint     samplerindex;  // index for sampler in bindless heap

    float    _pad0;
    float    _pad0;
};

// simple full-screen vertex input -> UV passthrough
struct VS_IN { float3 pos : POSITION; float2 uv : TEXCOORD0; };
struct PS_IN { float2 uv  : TEXCOORD0; };

// full-screen passthrough vertex shader
PS_IN VSFullscreen(VS_IN vin)
{
    PS_IN o;
    o.uv = vin.uv;
    return o;
}

// reconstruct view-space position from depth sample
float3 ReconstructViewPosition(float2 uv, float depth)
{
    // depth in [0..1] (DirectX). Convert to clip space -1..1 Z
    float4 clip = float4(uv * 2.0f - 1.0f, depth * 2.0f - 1.0f, 1.0f);
    float4 view = mul(clip, InvProjection); // clip->view
    view /= view.w;
    return view.xyz;
}

// fallback normal from depth (sobel-ish)
float3 ComputeNormalFromDepth(float2 uv)
{
    // use bindless sampling macros from BindlessResources.hlsl
    float d  = SampleBindlessTexture2D(depthindex, samplerindex, uv).r;
    float dpx = SampleBindlessTexture2D(depthindex, samplerindex, uv + float2(InvScreen.x, 0)).r;
    float dmx = SampleBindlessTexture2D(depthindex, samplerindex, uv - float2(InvScreen.x, 0)).r;
    float dpy = SampleBindlessTexture2D(depthindex, samplerindex, uv + float2(0, InvScreen.y)).r;
    float dmy = SampleBindlessTexture2D(depthindex, samplerindex, uv - float2(0, InvScreen.y)).r;

    // approximate view-space tangents -> normal
    float3 p  = ReconstructViewPosition(uv, d);
    float3 px = ReconstructViewPosition(uv + float2(InvScreen.x, 0), dpx);
    float3 py = ReconstructViewPosition(uv + float2(0, InvScreen.y), dpy);

    float3 n = normalize(cross(py - p, px - p));
    return n;
}

float4 PSHBAO(PS_IN pin) : SV_Target
{
    float2 uv = pin.uv;
    float depth = gDepth.Sample(samPoint, uv).r;
    if (depth >= 1.0f) // far plane -> no occlusion
        return float4(1,1,1,1);

    float3 pos = ReconstructViewPosition(uv, depth);
    float3 normal = float3(0,0,1);
    // prefer provided normal texture (assumed view-space packed in [0..1] -> remap)
    if (gNormal != NULL)
    {
        float3 nSample = gNormal.Sample(samPoint, uv).rgb;
        // assume normals encoded in [0..1]
        normal = normalize(nSample * 2.0f - 1.0f);
    }
    else
    {
        normal = ComputeNormalFromDepth(uv);
    }

    // random rotation per-pixel to reduce banding
    float2 rnd = gRand.Sample(samPoint, uv * NoiseScale).rg;
    float rot = rnd.x * 6.2831853; // [0,2pi)

    float ao = 0.0f;
    float invNumRays = 1.0f / max(1, NumRays);
    float invNumSteps = 1.0f / max(1, NumSteps);

    // convert radius in pixels to uv units
    float2 pxToUV = InvScreen;
    float radiusUV = RadiusPx * (pxToUV.x + pxToUV.y) * 0.5; // approximation

    // sweep rays
    for (int r = 0; r < NumRays; ++r)
    {
        // angle for this ray with rotation
        float ang = ( (float)r / (float)NumRays ) * 6.2831853 + rot;
        float2 dir = float2(cos(ang), sin(ang));

        // sample along the ray
        for (int s = 1; s <= NumSteps; ++s)
        {
            float frac = s * invNumSteps; // [0..1]
            float stepUV = radiusUV * frac;
            float2 sampleUV = uv + dir * stepUV;

            // stay inside
            if (sampleUV.x < 0.0f || sampleUV.x > 1.0f || sampleUV.y < 0.0f || sampleUV.y > 1.0f)
                break;

            float sampleDepth = gDepth.Sample(samPoint, sampleUV).r;
            if (sampleDepth >= 1.0f) // nothing at far plane
                continue;

            float3 samplePos = ReconstructViewPosition(sampleUV, sampleDepth);
            float3 v = samplePos - pos;
            float dist = length(v);
            if (dist <= 0.0001f) // ignore coincident
                continue;

            float3 dirToSample = v / dist;

            // geometric occlusion: if sample is in front (smaller z in view-space is closer to camera)
            // note: view-space Z is negative forward in many conventions; we use full vector comparison.
            // We consider an occluder if samplePos.z > pos.z + Bias (further from camera? depends on projection)
            // Use dot with view-forward (-Z) to detect occlusion along ray: occludes if sample is between viewer and current sample along ray.
            // Simpler robust test: project sample onto view-space sphere around pos using depth difference
            float depthDiff = (samplePos.z - pos.z);

            // if the sample is closer to the camera than the current point (i.e., positive occluder)
            // Here sign of z may be negative (camera looks down -Z). We therefore test by comparing distances along view forward.
            bool isOccluder = (dot(samplePos, float3(0,0,1)) < dot(pos, float3(0,0,1))) ? true : false;
            // More robust: treat occluder if length of v along view-forward is significant opposite sign:
            // We'll compute occlusion contribution based on how much sample projects above the horizon
            float nDot = dot(normal, dirToSample);
            // ignore back-facing directions
            if (nDot <= 0.0f) continue;

            // occlusion weight: angle * depth difference * falloff
            float occ = saturate(nDot - NormalBias);
            // additional attenuation by distance (so close samples contribute more)
            float atten = pow(saturate(1.0f - (dist / (RadiusPx * 0.02f + 1e-6f))), Falloff); // radius converts approx to view space
            // simple depth-based occlusion: if sample is significantly closer to camera along view-ray, count it
            float viewDepthDiff = pos.z - samplePos.z; // positive if sample is in front (occluding)
            float depthTerm = saturate(viewDepthDiff / max(0.0001f, dist));
            occ *= depthTerm * atten;

            ao += occ;
        }
    }

    // normalize
    float samplesTotal = (float)NumRays * (float)NumSteps;
    float aoAvg = ao / max(1.0f, samplesTotal);

    // final AO value: 1.0 = no occlusion, 0.0 = fully occluded
    float aoFinal = 1.0f - saturate(aoAvg * Intensity);

    return float4(aoFinal, aoFinal, aoFinal, 1.0f);
}
