#ifndef _FONT_SHADER_COMMON_INC_
#define _FONT_SHADER_COMMON_INC_

#ifdef FREE_TYPE_ALGO
struct FontVSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

cbuffer FontConstants
{
    float2  sdf_minmax;
    uint    color;
    int     buffer_index;
    int     buffer_offset;
    int     texture_index;
    float4x4    transform;
};

#else
struct FontVSOutput
{
    float4 pos : SV_Position;
    uint offset;
    uint contours;
    uint points;
};

cbuffer FontConstants
{
    uint color;
    int buffer_index;
    int buffer_offset;
    float4x4 transform;
};
#endif

struct FontPSOutput
{
    float4 color : SV_Target0;
};

#endif //_FONT_SHADER_COMMON_INC_