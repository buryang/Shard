#include "DDGICommon.hlsli"

/**
 * camera tracking window logic
 */
void InitTrackingWindow(float3 cam_pos)
{

} 

void UpdateTrackingWindow(float3 cam_pos, float3 win_center)
{
    float3 displacement = cam_pos - win_center;
    if(abs(displacement.x) > DDGI_LEAPFROG_THRESHOLD){
        //todo
    }
}

void ConvolveRaysToProbes()
{

}

//todo distribute jobs in wavefront
[numthreads(8, 8, 1)]
CSUpdateProbeAtlases(uint3 groupID : SV_GroupID, uint thread_index : SV_GroupIndex)
{
    //todo
}