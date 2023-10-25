#include "DebugViewCommon.hlsli"

float4 main(in GSOutput input) : SV_TARGET
{
    return input.color;
}