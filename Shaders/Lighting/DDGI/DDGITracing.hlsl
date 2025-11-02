#includ "DDGICommon.hlsli"

EProbeState QueryProbeState(float3 position)
{

}

void TraceProbeRays()
{
    //todo
}

[numthreads(8, 8, 1)]
void CSProbeTrace(uint3 groupID : SV_GroupID, uint thread_index : SV_GroupIndex)
{
    //todo
}