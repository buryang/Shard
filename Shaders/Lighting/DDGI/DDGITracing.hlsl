#include "DDGICommon.hlsli"
#include "../RayTrace/RTCommon.hlsli" 

EProbeState QueryProbeState(float3 position)
{

}

void TraceProbeRays()
{
    //screen-space ray tracing for ddgi probes
}

[numthreads(8, 8, 1)]
void CSProbeTrace(uint3 groupID : SV_GroupID, uint thread_index : SV_GroupIndex)
{
    //todo
}