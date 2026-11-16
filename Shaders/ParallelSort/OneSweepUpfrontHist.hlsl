#include "../ParallelSort.hlsli"

[numthreads(ONESWEEP_UPFRONT_DIM, 1, 1)]
void CSUpfrontHist(uint3 group_index : SV_GroupID, uint3 thread_index : SV_GroupThreadID)
{
    const uint hist_size = ONESWEEP_DIGIT_RADIX * 2;
    for (uint i = thread_index.x; i < hist_size; i += ONESWEEP_UPFRONT_DIM)
        gs_global_hist[i] = 0u;
    GroupMemoryBarrierWithGroupSync();
    BlockHistogramCount(group_index.x, thread_index.x);
    GroupMemoryBarrierWithGroupSync();
    GlobalHistogramReduceWrite(thread_index.x);
}
