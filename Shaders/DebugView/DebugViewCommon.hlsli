#ifndef _DEBUGVIEW_COMMON_INC_
#define _DEBUGVIEW_COMMON_INC_
#include "../CommonUtils.hlsli"

//todo pushconst or uniform buffer
struct VSParams
{
    matrix world_view_proj;
};

struct GSParams
{
    float2 render_target_size;
    float line_width;
};

struct VSInput
{
    float4 pos : POSITION0;
    float4 color;
};

struct GSInput
{
    float4 pos : POSITION0;
    float4 color;
};

struct GSOutput
{
    float4 pos : SV_POSITION;
    float4 color;
};

//for more infomation see: https://www.gamedev.net/forums/topic/609031-hlsl-line-geometry-shader/

#endif //_DEBUGVIEW_COMMON_INC_