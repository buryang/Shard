#include "../ParallelSort.hlsli"

cbuffer bitonic_sort_config : register(b1)
{
    uint sort_data_rw_bf_index;
    uint element_count;
    uint reverse;
    uint k;
    uint j;
};

#if USE_BITONIC_METHOD == BITONIC_INNER_METHOD
[numthreads(BITONIC_THREAD_GROUP_SIZE, 1, 1)]
void CSBitonicPreSort(uint3 group_id : SV_GroupID, uint thread_index : SV_GroupIndex)
{
    BitonicPreSort(GetBindlessRWByteBufferUniform(sort_data_rw_bf_index), element_count, reverse, group_id, thread_index);
}

[numthreads(BITONIC_THREAD_GROUP_SIZE, 1, 1)]
void CSBitonicInnerSort(uint3 group_id : SV_GroupID, uint thread_index : SV_GroupIndex)
{
    BitonicInnerSort(GetBindlessRWByteBufferUniform(sort_data_rw_bf_index), element_count, reverse, group_id, thread_index);
}
#else
[numthreads(256, 1, 1)]
void CSBitonicOuterSort(uint3 dispatch_id : SV_DispatchThreadID)
{
    BitonicOuterSort(GetBindlessRWByteBufferUniform(sort_data_rw_bf_index), element_count, k, j, reverse, dispatch_id);
}
#endif
