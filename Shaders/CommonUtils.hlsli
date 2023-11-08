#ifndef _COMMON_UTILS_INC_
#define _COMMON_UTILS_INC_

#define M_PI 3.14159265359
#define M_INV_PI 0.3183098861
#define M_E 2.71828182845
#define M_GLODEN_RATIO_CONJUGATE 0.61803398875

#define MEDIUMP_FLT_MIN 0.00006103515625
#define MEDIUMP_FLT_MAX 65504.0
#define SaturateMediump(x) min(x, MEDIUMP_FLT_MAX)

#ifdef SHADER_API_VULKAN
#define VK_PUSH_CONSTANT [[vk::push_constant]]
#define VK_BINDING(BIND, SET) [[vk::binding(BIND, SET)]]
#else 
#define VK_PUSH_CONSTANT
#define VK_BINDING(BIND, SET)
#endif

//from https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ACESFilmToneMap(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}
 
//https://www.shadertoy.com/view/7sBfDm
float3 LinearToSRGB(float3 rgb)
{
    rgb = saturate(rgb);
    return lerp(rgb * 12.92f, 1.055f*pow(rgb, float3(1.f / 2.4f)) - 0.055f, step(float3(0.0031308f), rgb));
}

float3 SRGBToLinear(float3 rgb)
{
    float3 lin_rgb = lerp(rgb / 12.92f, pow((rgb + 0.055f) / 1.055f, float3(2.4)), step(float3(0.04045f), rgb));
    return saturate(lin_rgb);
}

/*------------noise function---------------*/
//https://www.shadertoy.com/view/4djSRW
//https://github.com/everyoneishappy/happy.fxh.git
//---white noise---------
float Hash11(float p)
{
    p = frac(p * .1031f);
    p *= p + 33.33f;
    p *= p + p;
    return frac(p);
}

float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * .1031f);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float Hash13(float3 p3)
{
    p3 = frac(p3 * .1031f);
    p3 += dot(p3, p3.zyx + 31.32f);
    return frac((p3.x + p3.y) * p3.z);
}

/*---IGN(interleaved gradient noise) noise----------
 from "Next Generation Post Processing inCall of Duty Advanced Warfare":
 Experimenting and optimizing a noise generator, we found a noise function that we could classify 
 as being half way between dithered and random, and that we called Interleaved Gradient Noise
*/
float IGN(float2 p, int frame)
{
    p += float(frame) * 5.588238f;
    return frac(52.9829189f * frac(0.06711056f * float(p.x) + 0.00583715f * float(p.y)));
}
//----blue noise--------
//you can get free blue noise from here:http://momentsingraphics.de/BlueNoise.html
//sample blue noise should use nearest filter  


float Pow3(float x)
{
    return x * x * x;
}

float2 Pow3(float2 x)
{
    return x * x * x;
}

float3 Pow3(float3 x)
{
    return x * x * x;
}

float4 Pow3(float4 x)
{
    return x * x * x;
}

float Pow4(float x)
{
    float xx = x * x;
    return xx * xx;
}

float2 Pow4(float2 x)
{
    float2 xx = x * x;
    return xx * xx;
}

float3 Pow4(float3 x)
{
    float3 xx = x * x;
    return xx * xx;
}

float4 Pow4(float4 x)
{
    float4 xx = x * x;
    return xx * xx;
}

float Pow5(float x)
{
    float xx = x * x;
    return xx * xx * x;
}

float2 Pow5(float2 x)
{
    float2 xx = x * x;
    return xx * xx * x;
}

float3 Pow5(float3 x)
{
    float3 xx = x * x;
    return xx * xx * x;
}

float4 Pow5(float4 x)
{
    float4 xx = x * x;
    return xx * xx * x;
}

//HDR encode as: https://iwasbeingirony.blogspot.com/2010/06/difference-between-rgbm-and-rgbd.html
//use max range := 64 like ue
float4 RGBMEncode(float3 rgb)
{
    rgb *= 1.f / 64.f;
    float m = saturate(max(max(1e-6, rgb.r), max(rgb.g, rgb.b)));
    m = ceil(m * 255.f) / 255.f;
    return float4(rgb / m, m);
}

float3 RGBMDecode(float4 rgbm)
{
    return rgbm.rgb * (rgbm.a * 64.f);
}

float4 RGBDEncode(float3 rgb)
{
    float m = max(max(1e-6, rgb.r), max(rgb.g, rgb.b));
    m = max(64.f / m, 1.f);
    float d = saturate(floor(m) / 255.f);
    return float4(rgb * (d * (255.f / 64.f)), d);
}

float3 RGBDDecode(float4 rgbd)
{
    return rgbd.rgb * ((64.f / 255.f) / rgbd.a);
}


//morton code from:https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/
uint2 ExpandBits2D(uint2 x)
{
    x &= 0x0000ffff; // x = ---- ---- ---- ---- fedc ba98 7654 3210
    x = (x ^ (x << 8)) & 0x00ff00ff; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
    x = (x ^ (x << 4)) & 0x0f0f0f0f; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
    x = (x ^ (x << 2)) & 0x33333333; // x = --fe --dc --ba --98 --76 --54 --32 --10
    x = (x ^ (x << 1)) & 0x55555555; // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
    return x;
}

uint Morton2D(float2 xy)
{
    uint2 expand_xy = ExpandBits2D(uint2(xy));
    return (expand_xy.x << 1) + expand_xy.y;
}

//https://developer.nvidia.com/blog/thinking-parallel-part-iii-tree-construction-gpu/

//expand 10-bit integer to 30 bits
uint ExpandBits3D(uint3 v)
{
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

//calc morton code for the 3d coordinate located in uint cube[0,1]
uint Morton3D(float3 xyz)
{
    const float cube_scale = 1024.f;
    xyz = min(max(xyz * cube_scale, 0.f), cube_scale - 1.f);
    uint3 expand_xyz = ExpandBits3D(uint3(xyz));
    return (expand_xyz.x << 2) + (expand_xyz.y << 1) + expand_xyz.z;
}


#endif //_COMMON_UTILS_INC_