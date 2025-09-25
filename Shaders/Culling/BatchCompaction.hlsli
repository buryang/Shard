#ifndef _BATCH_COMPACTION_INC_
#define _BATCH_COMPACTION_INC_
#include "../ComputeShaderCommon.hlsli"

RWStructuredBuffer<uint64_t> compacted_offsets_counter_buffer;;

[numthreads(64, 1, 1)]
void CSBatchCompaction(uint3 groupID : SV_GroupID, uint thread_index : SV_GroupIndex)
{
    //todo
    
    uint prefix_offset = WavePrefixSum(xx);
    if(WaveIsFirstLane())
    {
        uint offset = WaveReadLaneAt(prefix_offset, WaveGetLaneCount() - 1)；
        uint64 offset_count = (offset << 32u) + 0u;
        InterLockedAdd(compacted_offsets_counter_buffer[0], offset_count, offset_count); 
    }
}


#endif //_BATCH_COMPACTION_INC_