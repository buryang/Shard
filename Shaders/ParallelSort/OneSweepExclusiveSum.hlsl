#include "../ParallelSort.hlsli"

[numthreads(ONESWEEP_EXCSUM_DIM, 1, 1)]
void CSGlobalExclusiveSum(uint3 groupID : SV_GroupID, uint3 group_threadID : SV_GroupThreadID)
{
    LoadInclusiveScan(groupID.x, group_threadID.x);

    if (WaveGetLaneCount() >= 16)
    {
        GlobalExclusiveSumKernelWarpGE16(groupID.x, group_threadID.x);
    }

    if (WaveGetLaneCount() < 16)
    {
        GlobalExclusiveSumKernelWarpLT16(groupID.x, group_threadID.x);
    }
}
