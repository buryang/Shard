#ifndef _COMMON_UTILS_INC_
#define _COMMON_UTILS_INC_

#include "Platform.hlsli"

//*************************************************************************************************************************************************************//
//In order to migrate to Workgraph in the future, encapsulate all implementations as functions as much as possible, and then call them in the entry function   //
//*************************************************************************************************************************************************************//

//In HLSL, you cannot directly embed a RWStructuredBuffer (or any resource type like buffers/textures) as a member of a struct 
#define BUFFER_REF(type_in) uint64 

#ifdef __cplusplus
typedef unsigned int uint;
typedef glm::vec4 float4;
typedef glm::mat4 float4x4;
#else
//fp16 https://therealmjp.github.io/posts/shader-fp16/
//You’ll also need to make sure that you’re not inadvertantly 
//using the half datatype, since by default this is still mapped to fp32 in HLSL! 
#define half min16float
#define half2 min16float2
#define half3 min16float3
#define half4 min16float4
#define half3x3 min16float3x3
#define half3x4 min16float3x4
#endif

//pack and unpack value to a integral， cfg format（shift:bits)
#define UNPACK_BITS(val, cfg) ((val>>(true ? cfg)) &((1<<(false ? cfg))-1))
#define PACK_FLAG(cfg, val) (val<<(true ? cfg))
#define PACK_MASK(cfg) (((1<<(false ? cfg))-1)<<(true ? cfg))

inline void PackBits(inout uint packed, uint value, uint pos, uint bits)
{
    uint value_mask = value & ((1 << bits) - 1);
    packed |= (value_mask << pos);
}

inline uint BitsMask(uint bits, uint shift)
{
    return ((1u << bits) - 1) << shift;
}

//insert low bitCount bits of insert at an offset in base
inline uint BitfieldInsert(uint mask, uint preserve, uint enable)
{
    return bfi_b32(enable, preserve, mask);
    //return (preserve & mask) | (enable & ~mask);
}

inline uint BitfieldExtract(uint packed, uint pos, uint bits)
{
    return bfe_u32(packed, pos, bits);
    //return (packed >> pos) & ((1 << bits) - 1);
}

uint3 UnpackBitsToUint3(uint packed, int3 component_bits)
{
    return uint3(BitfieldExtract(packed, 0, component_bits.x),
                BitfieldExtract(packed, component_bits.x, component_bits.y),
                BitfieldExtract(packed, component_bits.y + component_bits.x, component_bits.z));
}

/*shuffle bits from low to high*/
uint ShuffleBitsU32(uint high, uint low, uint shuffle_bits)
{
    shuffle_bits &= 31u;
    uint result = low >> shuffle_bits;
    result |= shuffle_bits > 0u ? (high >> (32u - shuffle_bits)) : 0u;
    return result;
}

uint FloatToInteger(float value, float scale)
{
    return uint(value * scale + 0.5);

}

/**
* optimize code according to "https://www.elopezr.com/the-art-of-packing-data/"
* for platform has special packfing instructions(shader model 6.6),using it
*/
#define RGBA_FLOAT_TO_UNORM8_SCALE 255
#define RGBA_FLOAT_TO_UNORM10_SCALE 1023
#define RGBA_FLOAT_TO_UNORM16_SCALE 65535
#define RGBA_FLOAT_TO_SNORM8_SCALE 127

/*color space pack/unpack*/
uint PackFloat4ToRGBA8Unorm(float4 value)
{
    uint4 uvalue = uint4(value * RGBA_FLOAT_TO_UNORM8_SCALE + 0.5);
#if PACKING_INTRINSICS_ENABLED
    return pack_u8(uvalue);
#else
    return (uvalue.a << 24) | (uvalue.b << 16) | (uvalue.g << 8) | uvalue.r;
#endif
}

float4 UnpackRGBA8UnormToFloat4(uint packed)
{
#if PACKING_INTRINSICS_ENABLED
    return float4(unpack_u8u32(packed)) / RGBA_FLOAT_TO_UNORM8_SCALE;
#else
    uint ri = packed & 0xff;
    uint gi = (packed >> 8) & 0xff;
    uint bi = (packed >> 16) & oxff;
    uint ai = packed >> 24;
    return float4(ri, gi, bi, ai) / RGBA_FLOAT_TO_INTEGER8_SCALE;
#endif
}

/**
*the minimum value means -1.0f (e.g. the 5-bit value 10000 maps to -1.0f). In addition, the second-minimum 
*number maps to -1.0f (e.g. the 5-bit value 10001 maps to -1.0f). There are thus two integer representations
*for -1.0f. There is a single representation for 0.0f, and a single representation for 1.0f
*/

//assume input is in the -1, 1 range, for both packing and unpacking. Clamp accordingly if the input is unknown
uint PackFloat4ToRGBA8Snorm(float4 value)
{
    int4 ivalue = int4(round(value * RGBA_FLOAT_TO_SNORM8_SCALE));
#if PACKING_INTRINSICS_ENABLED
    return (uint) pack_s8(ivalue);
#else
    return (uint) ((ivalue.a << 24) | (ivalue.b << 16) | (ivalue.g << 8) | ivalue.r);
#endif
}
 
float4 UnpackRGBA8SnormToFloat4(uint packed)
{
#if PACKING_INTRINSICS_ENABLED
    return float4(unpack_s8u32(packed)) / RGBA_FLOAT_TO_SNORM8_SCALE;
#else
    int ri = (int) (packed << 24) >> 24;
    int gi = (int) (packed << 16) >> 24;
    int bi = (int) (packed << 8) >> 24;
    int ai = (int) (packed << 0) >> 24;
    return float4(ri, gi, bi, ai) / RGBA_FLOAT_TO_SNORM8_SCALE;
#endif
}

uint PackFloat4ToR10G10B10A2Unorm(float4 unpacked)
{
    //todo rewrite using bitfieldinsert
    uint packed = FloatToInteger(unpacked.x, RGBA_FLOAT_TO_UNORM10_SCALE);
    packed |= FloatToInteger(unpacked.y, RGBA_FLOAT_TO_UNORM10_SCALE) << 10;
    packed |= FloatToInteger(unpacked.z, RGBA_FLOAT_TO_UNORM10_SCALE) << 20;
    packed |= FloatToInteger(unpacked.w, 3) << 30;
    return packed;
}

float4 UnpackR10G10B10A2ToFloat4(uint packed)
{
    //todo rewrite using bitfieldextract
    float4 unpacked;
    unpacked.x = float(packed & 0x3ffu) / RGBA_FLOAT_TO_UNORM10_SCALE;
    unpacked.y = float((packed >> 10) & 0x3ffu) / RGBA_FLOAT_TO_UNORM10_SCALE;
    unpacked.z = float((packed >> 20) & 0x3ffu) / RGBA_FLOAT_TO_UNORM10_SCALE;
    unpacked.w = float((packed >> 30) & 0x3u) / 3;
    return unpacked;
}

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
    float3 lin_rgb = lerp(rgb / 12.92f, pow((rgb + 0.055f) / 1.055f, float3(2.4f)), step(float3(0.04045f), rgb));
    return saturate(lin_rgb);
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
/**
\brief: input x 16-bit vec
*/
uint2 ExpandBits2D(uint2 x)
{
    x &= 0x0000ffffu; // x = ---- ---- ---- ---- fedc ba98 7654 3210
    x = (x ^ (x << 8)) & 0x00ff00ffu; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
    x = (x ^ (x << 4)) & 0x0f0f0f0fu; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
    x = (x ^ (x << 2)) & 0x33333333u; // x = --fe --dc --ba --98 --76 --54 --32 --10
    x = (x ^ (x << 1)) & 0x55555555u; // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
    return x;
}

uint2 ReverseBits2D(uint2 x)
{
    x &= 0x55555555u;
    x = (x ^ (x >> 1)) & 0x33333333u;
    x = (x ^ (x >> 2)) & 0x0f0f0f0fu;
    x = (x ^ (x >> 4)) & 0x00ff00ffu;
    x = (x ^ (x >> 8)) & 0x0000ffffu;
    return x;
}

uint Morton2D(float2 xy)
{
    uint2 expand_xy = ExpandBits2D(uint2(xy));
    return (expand_xy.x << 1) + expand_xy.y;
}

float2 ReverseMorton2D(uint x)
{
    float2 pixel = float2(ReverseBits2D(uint2(x, x >> 1)));
    return pixel;
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
    const float cube_scale = 1024.f; //remap to 10bit integer
    xyz = min(max(xyz * cube_scale, 0.f), cube_scale - 1.f);
    uint3 expand_xyz = ExpandBits3D(uint3(xyz));
    return (expand_xyz.x << 2) + (expand_xyz.y << 1) + expand_xyz.z; //todo + or | 
}

inline uint ExtractLSB(uint x)
{
    return x & -x;
}

inline uint ExtractMSB(uint x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x - (x >> 1);
}

//https://www.yosoygames.com.ar/wp/2018/03/vertex-formats-part-1-compression/
//https://rene.ruhr/gfx/adoption/ sample disk from square


#endif //_COMMON_UTILS_INC_