#include "DebugViewCommon.hlsli"

float4 Main(in GSOutput input) : SV_TARGET
{
    return input.color;
}