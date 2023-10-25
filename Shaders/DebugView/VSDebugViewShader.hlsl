#include "DebugViewCommon.hlsli"

VK_PUSH_CONSTANT
VSParams vertex_params;

void main( in VSInput input : POSITION0, out GSInput output) 
{
    output.pos = mul(input.pos, vertex_params.world_view_proj);
    output.color = input.color;
}