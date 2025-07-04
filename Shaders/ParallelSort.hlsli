#ifndef _PARALLEL_SORT_INC_
#define _PARALLEL_SORT_INC_

#include "Platform.hlsli"


//todo realize radix sort etc.
//https://gpuopen.com/learn/boosting_gpu_radix_sort
//https://gpuopen.com/download/Introduction_to_GPU_Radix_Sort.pdf
//https://research.nvidia.com/publication/2016-03_single-pass-parallel-prefix-scan-decoupled-look-back
//https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-39-parallel-prefix-sum-scan-cuda
//https://arxiv.org/pdf/1701.01189
//https://arxiv.org/pdf/2206.01784

#define ONESWEEP_THREAD_GROUP_SIZE 256
#define ONESWEEP_THREAD_ITEMS_COUNT 8
#define ONESWEEP_TILE_SIZE (ONESWEEP_THREAD_GROUP_SIZE*ONESWEEP_THREAD_ITEMS_COUNT)

#define ONESWEEP_STATUS_MASK 0xC0000000u
#define ONESWEEP_STATUS_SHIFT 30
#define ONESWEEP_COUNTER_MASK ~ONESWEEP_STATUS_MASK
#define ONESWEEP_STATUS_N 0 //not ready
#define ONESWEEP_STATUS_L 1 //local count
#define ONESWEEP_STATUS_G 2 //global sum
#define ONESWEEP_STATUS_R 3 //not used

//prefix sum utilities

void CSUpfrontHist()
{
    
}

void CSExclusiveSum()
{
    
}

[numthreads(,1,1)]
void CSChainedScanDigitBinning(uint3 group_threadID:SV_GroupThreadID)
{
    
}

//bitonic sort time complexity see https://en.wikipedia.org/wiki/Bitonic_sorter
//code merged from https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/Bitonic32PreSortCS.hlsl

#define BITONIC_INNER_METHOD 1 //load data to LDS and loop sort on GPU
#define BITONIC_OUTTER_METHOD 2 //sort directly in memory and sync/loop on cpu
//use outter loop for large array size bigger than 2048, while inner loop for others 
#define USE_BITONIC_METHOD BITONIC_INNER_METHOD

/*
*32bit: key; 64bit: key-index pair
*/
#define USE_64BIT_BITONICSORT 1 
#define BITONMIC_ELEMENT_MAX_NUM  2048 //sort for up to 2048 values
#define BITONIC_THREAD_GROUP_SIZE BITONMIC_ELEMENT_MAX_NUM/2 //use 1024 for thread group size
#define BITONIC_LDS_DATA_COUNT BITONMIC_ELEMENT_MAX_NUM
#define BITONIC_LDS_INDEX_MASK (BITONIC_LDS_DATA_COUNT-1)

#if USE_64BIT_BITONICSORT
#define ELEMENT_TYPE uint2
#else
#define ELEMENT_TYPE uint
#endif
#define BITONIC_ELEMENT_SIZE sizeof(ELEMENT_TYPE)

//determines if two sort keys should be swapped in the list.  NullItem is
//either 0 or 0xffffffff.  XOR with the NullItem will either invert the bits
//(effectively a negation) or leave the bits alone.  When the the NullItem is
//0, we are sorting descending, so when lhs < rhs, they should swap.  For an
//ascending sort, ~lhs < ~rhs should swap.
inline bool IsSwapNeeded(uint lhs, uint rhs, uint null_item)
{
    return (lhs ^ null_item) < (rhs ^ null_item);
}

inline bool IsSwapNeeded(uint2 lhs, uint2 rhs, uint null_item)
{
    return (lhs.y ^ null_item) < (rhs.y ^ null_item);
}

inline uint LDS_ELEMENT_INDEX(uint element_index)
{
    return element_index & BITONIC_LDS_INDEX_MASK;
}

//insert one bit at one_bit_mask position; one_bit_mask must have only one bit set
inline uint InsertOneBit(uint value, uint one_bit_mask)
{
    uint mask = one_bit_mask - 1;
    return (value & ~mask) << 1 | (value & mask) | one_bit_mask;
}

groupshared uint gs_sort_keys[BITONIC_LDS_DATA_COUNT];
#if USE_64BIT_BITONICSORT
groupshared uint gs_sort_indices[BITONIC_LDS_DATA_COUNT];
#endif

void LoadSortKey(RWByteAddressBuffer sort_data, uint element_index, uint total_element_count, ELEMENT_TYPE null_item)
{
    if (element_index < total_element_count)
    {
#if !USE_64BIT_BITONICSORT
        gs_sort_keys[LDS_ELEMENT_INDEX(element_index)] = sort_data.Load(element_index * BITONIC_ELEMENT_SIZE);
#else
        uint2 key_index_pair = sort_data.Load(element_index * BITONIC_ELEMENT_SIZE);
        gs_sort_keys[LDS_ELEMENT_INDEX(element_index)] = key_index_pair.y;
        gs_sort_indices[LDS_ELEMENT_INDEX(element_index)] = key_index_pair.x;
#endif

    }
    else
    {
#if !USE_64BIT_BITONICSORT
        gs_sort_keys[LDS_ELEMENT_INDEX(element_index)] = null_item;
#else
        gs_sort_keys[LDS_ELEMENT_INDEX(element_index)] = null_item.y;
        gs_sort_indices[LDS_ELEMENT_INDEX(element_index)] = null_item.x;
#endif
    }
}

void StoreSortKeyIndexPair(RWByteAddressBuffer sort_data, uint element_index, uint total_element_count)
{
    if (element_index < total_element_count)
    {
#if !USE_64BIT_BITONICSORT
        sort_data.Store(element_index * BITONIC_ELEMENT_SIZE, gs_sort_keys[LDS_ELEMENT_INDEX(element_index)]);
#else
        sort_data.Store2(element_index * BITONIC_ELEMENT_SIZE, uint2(gs_sort_keys[LDS_ELEMENT_INDEX(element_index)], gs_sort_indices[LDS_ELEMENT_INDEX(element_index)]));
#endif
    }
}

/**
*this function form a Bitonic Sequence from a random input 
*bitonic sort must sort the power-of-two ceiling of items. for example 391 items -> 512 items
*so presort do two things : 1)append null items as need 2) inital sort for k values up to 2048
*because we can fit 2048 key-indice in LDS with occupancy greater than one
*a single thread group can use as much as 32KB of LDS
*/
void BitonicPreSort(RWByteAddressBuffer sort_data, uint list_element_count, bool reverse, uint3 groupID : SV_GroupID, uint thread_index : SV_GroupIndex)
{
    const uint group_offset = groupID.x * BITONIC_LDS_DATA_COUNT;
    
#if USE_64BIT_BITONICSORT
    //descending: 0x0000000u; ascending: 0xffffffffu
    ELEMENT_TYPE null_item = reverse ? uint2(0x00000000u, 0x00000000u) : uint2(0xffffffffu, 0xffffffffu);
#else
    ELEMENT_TYPE null_item = reverse ? 0x00000000u : 0xffffffffu;
#endif 
    //todo whether to use unwarped thread_index
    thread_index = GetUnwarpedThreadIndex(thread_index, BITONIC_THREAD_GROUP_SIZE, THREAD_WARP_SIZE);
    LoadSortKey(sort_data, group_offset + thread_index, list_element_count, null_item);
    LoadSortKey(sort_data, group_offset + thread_index + BITONIC_THREAD_GROUP_SIZE, list_element_count, null_item);
    
    GroupMemoryBarrierWithGroupSync();
    
    uint k;
    [unroll]
    for (k = 2; k <= BITONIC_LDS_DATA_COUNT; k <<= 1)
    {
        for (uint j = k >> 1; j > 0; j >>= 1)
        {
            uint index2 = InsertOneBit(thread_index, j);
            uint index1 = index2 ^ (k == 2*j ? k-1 : j);
            
            uint lhs_val = gs_sort_keys[index1];
            uint rhs_val = gs_sort_keys[index2];
            
            if (IsSwapNeeded(lhs_val, rhs_val, null_item))
            {
                //swap work
                gs_sort_keys[index1] = rhs_val;
                gs_sort_keys[index2] = lhs_val;
#if USE_64BIT_BITONICSORT
                lhs_val = gs_sort_indices[index1];
                rhs_val = gs_sort_indices[index2];
                gs_sort_indices[index1] = rhs_val;
                gs_sort_indices[index2] = lhs_val;
#endif
            }
            
            GroupMemoryBarrierWithGroupSync();
        }

    }

    StoreSortKeyIndexPair(sort_data, group_offset + thread_index, list_element_count);
    StoreSortKeyIndexPair(sort_data, group_offset + thread_index + BITONIC_THREAD_GROUP_SIZE, list_element_count);

}

/**
* for large list first do the outter sorting by sorting groups of size k,
* starting with k=2 and doubling until k>=NumItems. To sort the
* group, keys are compared with a distance of j, which starts at half
* of k and continues halving down to 1(CPU use barriers/Fence to sync). 
* util j is 1024 and less, the compare and swap can happen in LDS(inner loop),
* inner sorting happens in LDS and loops.  Outer sorting
* happens in memory and does not loop. (Looping happens on the CPU by
* issuing sequential dispatches and barriers); so bitonic is multi pass
* cannot do sorting onesweep
*/

#if USE_BITONIC_METHOD == BITONIC_INNER_METHOD
void BitonicInnerSort(RWByteAddressBuffer sort_data, uint list_element_count, bool reverse, uint3 groupID : SV_GroupID, uint thread_index : SV_GroupIndex)
{
    const uint group_offset = groupID.x * BITONIC_LDS_DATA_COUNT;
#if USE_64BIT_BITONICSORT
    //descending: 0x0000000u; ascending: 0xffffffffu
    ELEMENT_TYPE null_item = reverse ? uint2(0x00000000u, 0x00000000u) : uint2(0xffffffffu, 0xffffffffu);
#else
    ELEMENT_TYPE null_item = reverse ? 0x00000000u : 0xffffffffu;
#endif 
    //load memory data to LDS
    //todo whether to use unwarped thread_index
    thread_index = GetUnwarpedThreadIndex(thread_index, BITONIC_THREAD_GROUP_SIZE, THREAD_WARP_SIZE);
    LoadSortKey(sort_data, group_offset + thread_index, list_element_count, null_item);
    LoadSortKey(sort_data, group_offset + thread_index + BITONIC_THREAD_GROUP_SIZE, list_element_count, null_item);
    
    GroupMemoryBarrierWithGroupSync();

    [unroll]
    for (uint j = BITONIC_THREAD_GROUP_SIZE; j > 0; j >>= 1)
    {
        //FIX when j < warp_size do we need wave intrinsics to opt it
        uint index2 = InsertOneBit(thread_index, j);
        uint index1 = index2 ^ j;
        
        uint lhs_val = gs_sort_keys[index1];
        uint rhs_val = gs_sort_keys[index2];
        
        if(IsSwapNeeded(lhs_val, rhs_val, null_item))
        {
            gs_sort_keys[index1] = rhs_val;
            gs_sort_keys[index2] = lhs_val;
#if USE_64BIT_BITONICSORT
            lhs_val = gs_sort_indices[index1];
            rhs_val = gs_sort_indices[index2];
            gs_sort_indices[index1] = rhs_val;
            gs_sort_indices[index2] = lhs_val;
#endif
        }
        GroupMemoryBarrierWithGroupSync();
    }
    
    StoreSortKeyIndexPair(group_offset + thread_index, list_element_count);
    StoreSortKeyIndexPair(group_offset + thread_index + BITONIC_THREAD_GROUP_SIZE, list_element_count);
}
#elif USE_BITONIC_METHOD == BITONIC_OUTTER_METHOD 
//k >= 4096, j >= 2048 && j < k
void BitonicOuterSort(RWByteAddressBuffer sort_data, uint list_element_count, uint k, uint j, bool reverse, uint3 dispatchID : SV_DispatchThreadID)
{
    uint index2 = InsertOneBit(dispatchID.x, j);
    uint index1 = index2 ^(k == 2 * j ? k - 1 : j);
    
    if(index2 >= list_element_count)
        return;
#if USE_64BIT_BITONICSORT
#define LoadElement(idx) sort_data.Load2(idx*BITONIC_ELEMENT_SIZE);
#define StoreElement(idx, element) sort_data.Store2(idx*BITONIC_ELEMENT_SIZE, element);
    //descending: 0x0000000u; ascending: 0xffffffffu
    ELEMENT_TYPE null_item = reverse ? uint2(0x00000000u, 0x00000000u) : uint2(0xffffffffu, 0xffffffffu);
#else
#define LoadElement(idx) sort_data.Load(idx*BITONIC_ELEMENT_SIZE);
#define StoreElement(idx, element) sort_data.Store(idx*BITONIC_ELEMENT_SIZE, element);
    ELEMENT_TYPE null_item = reverse ? 0x00000000u : 0xffffffffu;
#endif
    ELEMENT_TYPE lhs_val = LoadElement(index1);
    ELEMENT_TYPE rhs_val = LoadElement(index2);

    if(IsSwapNeeded(lhs_val, rhs_val, null_item)
    {
        StoreElement(index1, rhs_val);
        StoreElement(index2, lhs_val);
    }
}
#else
#error "not supported bitonic loop method"
#endif

#endif //_PARALLEL_SORT_INC_