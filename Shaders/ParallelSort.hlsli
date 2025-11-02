#ifndef _PARALLEL_SORT_INC_
#define _PARALLEL_SORT_INC_

#include "Platform.hlsli"
#include "BindlessResource.hlsli"

//compiler define 
//COMPILE_EXCLUSIVE_SUM
//COMPILE_DIGIT_BINNING

#pragma multi_compile USE_64BIT_BITONICSORT _
#pragma multi_compile USE_16BIT_ONESWEEP _
#pragma multi_compile USE_PAIR_ONESWEEP _ 
#pragma multi_compile ONESWEEP_KEY_UINT ONESWEEP_KEY_FLOAT ONESWEEP_KEY_INT
#pragma multi_compile ONESWEEP_PAYLOAD_UINT ONESWEEP_PAYLOAD_FLOAT ONESWEEP_PAYLOAD_INT
#pragma use_dxc

#pragma require wavebasic
#pragma require waveballot


/*
* key data type help utils
* http://stereopsis.com/radix.html by Michael Herf
* float directly conveted to uint is not suit for sorting
* so flip it to a unique value which can inverse remap back
*/
inline uint ConvertFloatToUint(float f)
{
    uint mask = -((int) (asuint(f) >> 31)) | 0x80000000u;
    return asuint(f) ^ mask;
}

inline float InvConvertUintToFloat(uint u)
{
    uint mask = -((u >> 31) - 1) | 0x80000000u;
    return asfloat(u ^ mask);
}

//flip sign bit to ensure negative number smaller than positive ones
inline uint ConvertIntToUint(int i)
{
    return asuint(i ^ 0x80000000u);
}

inline int InvConvertUintToInt(uint u)
{
    return asint(u ^ 0x80000000u);
}

//reverse order sort help func
inline uint CondFlipBitsUint(uint u, bool flip)
{
    return flip ? u ^ 0xffffffffu : u;
}

//realize radix sort, papers related
//https://gpuopen.com/learn/boosting_gpu_radix_sort
//https://gpuopen.com/download/Introduction_to_GPU_Radix_Sort.pdf
//https://research.nvidia.com/publication/2016-03_single-pass-parallel-prefix-scan-decoupled-look-back
//https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-39-parallel-prefix-sum-scan-cuda
//https://arxiv.org/pdf/2206.01784

/*
* WMS related argument, tile_size/window_size(warp), see https://arxiv.org/pdf/1701.01189 for details,
* local data shared in a tile, for example local histogram/local prefix sum(offset)/local reorder etc.
* thread block local histogram accumulate with global execlusive prefix sum to scatter
* follow realization from GPUSorting
*/

//digit place size define 
#define ONESWEEP_DIGIT_RADIX 256u
#define ONESWEEP_DIGIT_RADIX_MASK (ONESWEEP_DIGIT_RADIX - 1)
#define ONESWEEP_DIGIT_HALF_RADIX (ONESWEEP_DIGIT_RADIX >> 1)
#define ONESWEEP_DIGIT_HALF_MASK (ONESWEEP_DIGIT_HALF_RADIX - 1)
#define ONESWEEP_DIGIT_LOG2  8u //log2(ONESWEEP_DIGIT_RADIX) bits for digit
#define ONESWEEP_DIGIT_PLACES 4u //ceil(sizeof(uint)/ONESWEEP_DIGIT_LOG2)
#define ONESWEEP_BINNING_ITERS ONESWEEP_DIGIT_PLACES
#define ONESWEEP_DIGIT_PASSES 4u //(key width)/ONESWEEP_DIGIT_LOG2
#define ONESWEEP_BINNING_SIZE (1 << ONESWEEP_DIGIT_SIZE)

//global histogram offset for each radix
#define ONESWEEP_SEC_RADIX_OFFSET 256u 
#define ONESWEEP_THIRD_RADIX_OFFSET 512u
#define ONESWEEP_FOURTH_RADIX_OFFSET 768u 

/*one tile per block, so the tile size is product of threads per block & items per thread*/
#define ONESWEEP_UPFRONT_DIM 128 
#define ONESWEEP_UPFRONT_KEY_COUNT 8
#define ONESWEEP_UPFRONT_PER_HIST_THREADS (ONESWEEP_UPFRONT_DIM/2)
#define ONESWEEP_UPFRONT_TILE_SIZE (ONESWEEP_UPFRONT_DIM*ONESWEEP_UPFRONT_KEY_COUNT)

#define ONESWEEP_EXCSUM_DIM ONESWEEP_DIGIT_RADIX

#define ONESWEEP_DIGITBIN_DIM 256
#define ONESWEEP_DIGITBIN_ITEM_COUNT 21
#define ONESWEEP_DIGITBIN_TILE_SIZE (ONESWEEP_DIGITBIN_THREAD_COUNT*ONESWEEP_DIGITBIN_THREAD_COUNT)

//how mush time should we allow thread locks to wait on preceeding tiles? for looking back
#define ONESWEEP_MAX_SPIN_COUNT 1

#if 0
#define ONESWEEP_STATUS_MASK 0xC0000000u
#define ONESWEEP_STATUS_SHIFT 30
#define ONESWEEP_COUNTER_MASK ~ONESWEEP_STATUS_MASK
#define ONESWEEP_STATUS_N (0 << ONESWEEP_STATUS_SHIFT) //not ready
#define ONESWEEP_STATUS_L (1 << ONESWEEP_STATUS_SHIFT) //local count
#define ONESWEEP_STATUS_G (2 << ONESWEEP_STATUS_SHIFT) //global sum
#define ONESWEEP_STATUS_R (3 << ONESWEEP_STATUS_SHIFT) //not used
#else // for 16bit compatible
#define ONESWEEP_STATUS_MASK 0x0000000Cu
#define ONESWEEP_COUNTER_MASK ~ONESWEEP_STATUS_MASK
#define ONESWEEP_COUNTER_SHIFT 2
#define ONESWEEP_STATUS_N 0 //not ready
#define ONESWEEP_STATUS_L 1 //local count
#define ONESWEEP_STATUS_G 2 //global sum
#define ONESWEEP_STATUS_R 3 //not used
#endif


#define ONESWEEP_PART_SIZE 1792u
/**
* shared memory size for digit binning, smem[ONESWEEP_SMEM_SIZE-1] to store partition_index
* smem[0] to store
*/
#define ONESWEEP_SMEM_SIZE 4096u

//wave flags bits and mask
#define ONESWEEP_WFLAGS_BITS 32u //sizeof(uint)
#define ONESWEEP_WFLAGS_MASK (ONESWEEP_WFLAGS_BITS-1)


//compiler defines
//#define USE_64BIT_ONESWEEP 1 //0 for key-only 1 for key-pair
#if COMPILE_EXCLUSIVE_SUM
cbuffer onesweep_exclusive_sum_config
{
    uint thread_per_block;
    uint is_reverse;
    uint global_hist_rw_bf_index;  //device level offsets for each binning pass
    uint pass_hist_ch_rw_bf_index; //store reduced sum of partition titles
};
#elif 1// COMPILE_DIGIT_BINNING
cbuffer onesweep_digit_binning_config
{
    uint num_keys;
    uint radix_shift; //shouid be divisible by ONESWEEP_DIGIT_LOG2
    uint thread_blocks;
    uint is_Tail;
    uint is_reverse;
    
    //buffer bindless index
    uint sort_key_rw_bf_index;
    uint alt_key_rw_bf_index;
 
#ifdef USE_PAIR_ONESWEEP
    uint sort_payload_rw_bf_index;
    uint alt_payload_rw_bf_index;
#endif
    
    uint counter_ch_rw_bf_index; //atomiclly assign prtition tile index /global counter, each item for one pass 
};

struct LocalKeys
{
    uint keys[ONESWEEP_DIGITBIN_ITEM_COUNT];
};

struct LocalOffsets
{
#ifdef USE_16BIT_ONESWEEP
    uint16_t offsets[ONESWEEP_DIGITBIN_ITEM_COUNT];
#else
    uint offsets[ONESWEEP_DIGITBIN_ITEM_COUNT];
#endif
};

struct LocalDigits
{
#ifdef USE_16BIT_ONESWEEP
    uint16_t digits[ONESWEEP_DIGITBIN_ITEM_COUNT];
#else
    uint digits[ONESWEEP_DIGITBIN_ITEM_COUNT];
#endif
};

#endif 

inline uint GetBlockWavePass()
{
    return ONESWEEP_DIGITBIN_DIM / WaveGetLaneCount();
}

inline uint ExtractDigit(uint val, uint radix_shift)
{
    return (val >> radix_shift) & ONESWEEP_DIGIT_RADIX_MASK;
}

/**
* packed index layout
* index|shift_bit 
*/
inline uint ExtractPackedIndex(uint key, uint radix_shift)
{
    return key >> (radix_shift + 1) & ONESWEEP_DIGIT_HALF_MASK;

}

inline uint ExtractPackedShift(uint key, uint radix_shift)
{
    return (key >> radix_shift & 0x1) ? 16 : 0;
}

inline uint ExtractPackedValue(uint packed, uint key, uint radix_shift)
{
    return packed >> ExtractPackedShift(key, radix_shift) & 0xffffu;
}

/*half of threads in a block share one hist, so size of double of radix*/
groupshared uint4 gs_global_hist[ONESWEEP_DIGIT_RADIX * 2]; //shared memory for global histogram
groupshared uint gs_scan[ONESWEEP_DIGIT_RADIX]; //shared memory for scan
/*chained-scan kernel*/
groupshared uint gs_data[ONESWEEP_SMEM_SIZE]; //shared memory for digital binning and downsweep

void BlockHistogramCount(uint group_index, uint thread_index)
{
    const uint hist_offset = thread_index / ONESWEEP_UPFRONT_PER_HIST_THREADS * ONESWEEP_DIGIT_RADIX; //half of threads to a histogram
    const uint partition_end = (group_index == thread_per_block - 1) ? num_keys : (group_index + 1) * ONESWEEP_UPFRONT_TILE_SIZE;
    
    uint t;
    for (uint i = thread_index + group_index * ONESWEEP_UPFRONT_TILE_SIZE; i < partition_end; i += ONESWEEP_UPFRONT_DIM) //deal with data tile
    {
        //load data to hist from global memory
#if defined(ONESWEEP_KEY_UINT)
            t = xx;
#elif defined(ONESWEEP_KEY_INT)
            t = ConvertIntToUint(xx);
#elif defined(ONESWEEP_KEY_FLOAT)
            t = ConvertFloatToUint(xx);
#endif 
        InterlockedAdd(gs_global_hist[hist_offset + ExtractDigit(t, 0)].x, 1);
        InterlockedAdd(gs_global_hist[hist_offset + ExtractDigit(t, 8)].y, 1);
        InterlockedAdd(gs_global_hist[hist_offset + ExtractDigit(t, 16)].z, 1);
        InterlockedAdd(gs_global_hist[hist_offset + ExtractDigit(t, 24)].w, 1);
    }

}

//write local histogram to global memory
void GlobalHistogramReduceWrite(uint thread_index)
{
    RWByteAddressBuffer global_hist = Get
    for (uint i = thread_index; i < ONESWEEP_DIGIT_RADIX; i += ONESWEEP_UPFRONT_DIM)
    {
        InterlockedAdd(global_hist[i], gs_global_hist[i].x + gs_global_hist[i + ONESWEEP_DIGIT_RADIX].x);
        InterlockedAdd(global_hist[i + ONESWEEP_SEC_RADIX_OFFSET], gs_global_hist[i].y + gs_global_hist[i + ONESWEEP_DIGIT_RADIX].y);
        InterlockedAdd(global_hist[i + ONESWEEP_THIRD_RADIX_OFFSET], gs_global_hist[i].z + gs_global_hist[i + ONESWEEP_DIGIT_RADIX].z);
        InterlockedAdd(global_hist[i + ONESWEEP_FOURTH_RADIX_OFFSET], gs_global_hist[i].w + gs_global_hist[i + ONESWEEP_DIGIT_RADIX].w);
    }
}

//prefix sum utilities
[numthreads(ONESWEEP_UPFRONT_DIM, 1, 1)]
void CSUpfrontHist(uint group_index, uint thread_index)
{
    const uint hist_size = ONESWEEP_DIGIT_RADIX * 2;
    //clear shared memory
    for (uint i = thread_index; i < hist_size; i += ONESWEEP_UPFRONT_DIM)
        gs_global_hist[i] = 0u;
    GroupMemoryBarrierWithGroupSync();
    BlockHistogramCount(group_index, thread_index);
    GroupMemoryBarrierWithGroupSync();
    GlobalHistogramReduceWrite(thread_index);
}

//load ans scan in one warp
inline void LoadInclusiveScan(uint group_index, uint thread_index)
{
    RWByteAddressBuffer global_hist = xx; 
    const uint t = global_hist.Load(thread_index + group_index * ONESWEEP_DIGIT_RADIX);
    gs_scan[t] = t + WavePrefixSum(t);
}

//when warp size bigger than 16 need less barrier 
inline void GlobalExclusiveSumKernelWarpGE16(uint group_index, uint thread_index)
{
    const uint wave_count = WaveGetLaneCount();
    GroupMemoryBarrierWithGroupSync();
    if (thread_index < (ONESWEEP_DIGIT_RADIX / wave_count))
    {
        //inclusive sum in warp
        gs_scan[(thread_index + 1) * wave_count - 1] +=
            WavePrefixSum(gs_scan[(thread_index + 1) * wave_count - 1]);
    }
    
    GroupMemoryBarrierWithGroupSync();
    const uint lane_mask = wave_count - 1;
    const uint index = (thread_index & ~lane_mask) + (WaveGetLaneIndex() + 1 & lane_mask); //1&lane_mask ?
    GetCoherentBindlessRWByteBuffer(pass_hist_ch_rw_bf_index)[index + group_index * ONESWEEP_DIGIT_RADIX * thread_per_block] =
                                        ((WaveGetLaneIndex() != lane_mask ? gs_scan[thread_index] : 0) + (thread_index >= WaveGetLaneCount() ? WaveReadLaneAt(gs_scan[thread_index - 1], 0) : 0)) | ONESWEEP_STATUS_G;

}

inline void GlobalExclusiveSumKernelWarpLT16(uint group_index, uint thread_index)
{
    RWByteAddressBuffer pass_hist = GetCoherentBindlessRWByteBuffer(pass_hist_ch_rw_bf_index);
    const uint pass_hist_offset = group_index * ONESWEEP_EXCSUM_DIM * thread_per_block;
    if (thread_index < WaveGetLaneCount())
    {
        const uint circular_lane_shift = WaveGetLaneIndex() + 1 & WaveGetLaneCount() - 1;
        pass_hist[circular_lane_shift + pass_hist_offset] = (circular_lane_shift ? gs_scan[thread_index] : 0) | ONESWEEP_STATUS_G;
    }
    GroupMemoryBarrierWithGroupSync();
    
    const uint log2_lane = firstbithigh(WaveGetLaneCount());
    uint offset = log2_lane;
    uint j = WaveGetLaneCount();
    for (; j < (ONESWEEP_DIGIT_RADIX >> 1); j<<= log2_lane)
    {
        if(thread_index < (ONESWEEP_DIGIT_RADIX >> offset))
        {
            gs_scan[((thread_index + 1) << offset) - 1] += WavePrefixSum(gs_scan[((thread_index + 1) << offset) - 1]);
        }
        GroupMemoryBarrierWithGroupSync();
        
        if ((thread_index & ((j << log2_lane) - 1)) >= j)
        {
            if(thread_index < (j << log2_lane))
            {
                pass_hist[thread_index + pass_hist_offset] = (WaveReadLaneAt(gs_scan[((thread_index >> offset) << offset) - 1], 0) +
                                                                 ((thread_index & (j - 1)) ? gs_scan[thread_index - 1] : 0)) | ONESWEEP_STATUS_G;
            }
            else
            {
                if((thread_index+1)&(j-1))
                {
                    gs_scan[thread_index] += WaveReadLaneAt(gs_scan[((thread_index >> offset) << offset) - 1], 0);
                }
            }
        }
        offset += log2_lane;
    }
    GroupMemoryBarrierWithGroupSync();
    
    //if radix is not power of lanecount
    const uint index = thread_index + j;
    if (index < ONESWEEP_DIGIT_RADIX)
    {
        pass_hist[index + pass_hist_offset] = (WaveReadLaneAt(gs_scan[((index >> offset) << offset) - 1], 0) +
                                                        ((index & (j - 1)) ? gs_scan[index - 1] : 0)) | ONESWEEP_STATUS_G;
    }
}

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

inline uint GetScanCounterStatus(uint counter)
{
    return (counter & ONESWEEP_STATUS_MASK) >> ONESWEEP_STATUS_SHIFT;
}

inline uint GetScanCounterValue(uint counter)
{
    return counter & ONESWEEP_COUNTER_MASK;
}

//ensure tiles processed in order
inline uint GetPartitionTileIndex(uint thread_index)
{
    if(!thread_index)
        InterlockedAdd(GetCoherentBindlessRWByteBuffer(counter_ch_rw_bf_index), 1, gs_data[ONESWEEP_SMEM_SIZE - 1]);
    GroupMemoryBarrierWithGroupSync();
    return gs_data[ONESWEEP_SMEM_SIZE - 1];
}

inline uint GetCurrentPassIndex()
{
    return radix_shift / ONESWEEP_DIGIT_LOG2;
}

inline uint GetPartitonPassHistOffset(uint partition_index)
{
    return (GetCurrentPassIndex() * thread_blocks + partition_index) * ONESWEEP_DIGIT_RADIX;
}

inline uint GetGlobalPassHistOffset()
{
    return radix_shift * ONESWEEP_DIGITI_RADIX_BITS;
}

inline uint GetSubPartSizeWarpGE16()
{
    return ONESWEEP_DIGITBIN_ITEM_COUNT * WaveGetLaneCount();
}

inline uint GetThreadKeysSharedMemoryOffsetWarpGE16(uint thread_index)
{
    return WaveGetLaneIndex() + GetWaveIndex(thread_index) * GetSubPartSizeWarpGE16();
}

inline uint GetThreadKeysDeviceOffsetWarpGE16(uint thread_index, uint partition_index)
{
    return GetThreadKeysSharedMemoryOffsetWarpGE16(thread_index) + partition_index * ONESWEEP_PART_SIZE;
}

inline uint GetSubPartSizeWarpLT16(uint iterations)
{
    return ONESWEEP_DIGITBIN_ITEM_COUNT * WaveGetLaneCount() * iterations;
}

inline uint GetThreadKeysSharedMemoryOffsetWarpLT16(uint thread_index, uint iterations)
{
    return WaveGetLaneIndex() + (GetWaveIndex(thread_index) / iterations * GetSubPartSizeWarpLT16(iterations))
                                + (GetWaveIndex(thread_index) % iterations * WaveGetLaneCount());
}

inline uint GetThreadKeysDeviceOffsetWarpLT16(uint thread_index, uint partition_index, uint iterations)
{
    return GetThreadKeysSharedMemoryOffsetWarpLT16(thread_index, iterations) + partition_index * ONESWEEP_PART_SIZE:
}

inline void ClearWaveHist(uint thread_index)
{
    const uint hists_end = WaveGetLaneCount() > 16 ? 0 : 0;
    for (uint i = thread_index; i < hists_end; i += ONESWEEP_DIGITBIN_DIM)
        gs_data[i] = 0u;
}

inline void LoadKey(inout uint key, uint device_index)
{
    uint t = xx;
#if defined(ONESWEEP_KEY_INT)
    t = ConvertIntToUint(t);
#elif defined(ONESWEEP_KEY_FLOAT)
    t = ConvertFloatToUint(t);
#endif 
    key = CondFlipBitsUint(t, is_reverse);
}

inline void LoadDummyKey(inout uint key)
{
    key = is_reserve ? 0x00000000u : 0xffffffffu;
}

inline void StoreKey(uint device_index, uint gs_index)
{
    RWByteAddressBuffer key_buffer = xx;
    uint key = CondFlipBitsUint(gs_data[gs_index], is_reverse);
#if defined(ONESWEEP_KEY_UINT)
    key_buffer[device_index] = key;
#elif defined(ONESWEEP_KEY_INT)
    key_buffer[device_index] = InvConvertUintToInt(key);
#elif defined(ONESWEEP_KEY_FLOAT)
    key_buffer[device_index] = InvConvertUintToFloat(key);
#endif 
}

inline void LoadPayload(inout uint payload, uint device_index)
{
    payload = asuint(xx[device_index]);
}

inline void StorePayload(uint device_index, uint gs_index)
{
#ifdef ONESWEEP_PAYLOAD_UINT
    xx[device_index] = gs_data[gs_index];
#elif defined(ONESWEEP_PAYLOAD_INT)
    xx[device_index] = asint(gs_data[gs_index]);
#elif defined(ONESWEEP_PAYLOAD_FLOAT)
    xx[device_index] = asfloat(gs_data[gs_index]);
#endif
}

inline void DeviceBroadcastReductionsWarpGE16(uint thread_index, uint partition_Index, uint hist_reduction)
{
    RWByteAddressBuffer pass_hist = GetCoherentBindlessRWByteBuffer(pass_hist_ch_rw_bf_index);
    
    if (partition_Index < thread_blocks - 1)
    {
        InterlockedAdd(pass_hist[thread_index + PassHistOffset(partition_Index + 1)], hist_reduction | ONESWEEP_STATUS_L);
    }
}

inline void DeviceBroadcastReductionWarpLT16(uint thread_index, uint partition_Index, uint hist_reduction)
{
    RWByteAddressBuffer pass_hist = GetCoherentBindlessRWByteBuffer(pass_hist_ch_rw_bf_index);
    
    if (partition_Index < thread_blocks - 1)
    {
        InterlockedAdd(pass_hist[(thread_index << 1) + PassHistOffset(partition_Index + 1)], (hist_reduction & 0xffffu)|ONESWEEP_STATUS_L);
        InterlockedAdd(pass_hist[(thread_index << 1) + 1 + PassHistOffset(partition_Index + 1)], (hist_reduction >> 16) & 0xffffu)|ONESWEEP_STATUS_L);
    }
}

//one thread to accumulate hist[thread_index] and lookback
inline void LookBack(uint thread_index, uint part_index, uint exclusive_hist_reduction)
{
    RWByteAddressBuffer pass_hist = GetCoherentRWByteBuffer(counter_ch_rw_bf_index);
    if (thread_index < ONESWEEP_DIGIT_RADIX) //one thread for one digit histogram
    {
        uint look_back_reduction = 0u;
        for (uint k = part_index; k >= 0u;)
        {
            const uint counter_status = pass_hist[thread_index + GetPartitonPassHistOffset(k)];
            if ((counter_status & ONESWEEP_STATUS_MASK) == ONESWEEP_STATUS_G)
            {
                look_back_reduction += counter_status >> ONESWEEP_COUNTER_SHIFT;
                if (part_index < thread_blocks - 1)
                {
                    //sync to global
                    InterlockedAdd(pass_hist[thread_index + GetPartitionTileIndex(part_index)], (look_back_reduction << ONESWEEP_COUNTER_SHIFT) | ONESWEEP_STATUS_G);

                }
                gs_data[thread_index + part_index] = look_back_reduction - exclusive_hist_reduction;
            }
            
            if((counter_status&ONESWEEP_STATUS_MASK) == ONESWEEP_STATUS_L)
            {
                look_back_reduction += counter_status >> ONESWEEP_COUNTER_SHIFT;
                k--;
            }

        }

    }
}


inline void InitializeLookBackFallBack(uint thread_index)
{
    if(thread_index == 0)
        gs_data[thread_index] = 0u;
    
    if(thread_index == 1)
        gs_data[thread_index] = 1u;
}

inline void FallBackHistogram(uint index)
{
    uint key;
    LoadKey(key, index);
    InterlockedAdd(gs_data[ExtractDigit(key, radix_shift) + ONESWEEP_DIGIT_RADIX], 1);
}

//threads with same hread_index & ONESWEEP_DIGIT_RADIX_MASK value to accumulate and lookback
inline void LookBackWithFallBack(uint thread_index, uint part_index, uint exclusive_hist_reduction)
{
    uint spin_count = 0u;
    uint look_back_reduction = 0u;
    uint look_back_complete = thread_index < xx ? false : true;
    bool warp_look_back_complete = thread_index < xx ? false : true;
    uint look_back_index = GetPartitonPassHistOffset(part_index) + (thread_index & ONESWEEP_DIGIT_RADIX_MASK); //current partition's hist[thread_index] 
    const uint warp_size = WaveGetLaneCount();
    
    RWByteAddressBuffer pass_hist = GetCoherentRWByteBuffer(counter_ch_rw_bf_index);
    
    while (gs_data[0] < ONESWEEP_DIGIT_RADIX / WaveGetLaneCount()) //all radix acc finished
    {
        uint counter_status;
        if (!warp_look_back_complete)
        {
            if(!look_back_complete)
            {
                while (spin_count < ONESWEEP_MAX_SPIN_COUNT)
                {
                    counter_status = pass_hist[look_back_index];
                    if ((counter_status & ONESWEEP_STATUS_MASK) > ONESWEEP_STATUS_N)
                    {
                        break;
                    }
                    spin_count++;

                }
            }
            
            //avoid deadlock
            if (WaveActiveAnyTrue(spin_count == ONESWEEP_MAX_SPIN_COUNT))
                InterlockedOr(gs_data[1], 1);

        }
        
        GroupMemoryBarrierWithGroupSync();
        
        if (gs_data[1]) //fallback
        {
            if(thread_index < ONESWEEP_DIGIT_RADIX)
                gs_data[thread_index + ONESWEEP_DIGIT_RADIX] = 0u;
            GroupMemoryBarrierWithGroupSync();
            if(!thread_index)
                gs_data[1] = 0;
            const uint fall_back_end = ((look_back_index >> ONESWEEP_DIGIT_LOG2) - thread_blocks * GetCurrentPassIndex()) * ONESWEEP_PART_SIZE;
            const uint fall_back_begin = fall_back_end - ONESWEEP_PART_SIZE;
            for (uint i = thread_index + fall_back_begin; i < fall_back_end; i += ONESWEEP_DIGITBIN_DIM)
                FallBackHistogram(i);
            GroupMemoryBarrierWithGroupSync();
            
            uint reduce_out;
            if(thread_index < ONESWEEP_DIGIT_RADIX)
            {
                InterlockedCompareExchange(pass_hist[thread_index + GetPartitonPassHistOffset((look_back_index >> ONESWEEP_DIGIT_LOG2) - thread_blocks * GetCurrentPassIndex())],
                                                0u, gs_data[thread_index + ONESWEEP_DIGIT_RADIX] | ONESWEEP_STATUS_L, reduce_out);

            }
            
            if(!look_back_complete)
            {
                if ((reduce_out & ONESWEEP_STATUS_MASK) == ONESWEEP_STATUS_G)
                {
                    look_back_reduction += reduce_out >> ONESWEEP_COUNTER_SHIFT;
                    if(part_index < thread_blocks - 1)
                    {
                        InterlockedAdd(pass_hist[thread_index + GetPartitonPassHistOffset(part_index + 1)], 1 | look_back_reduction << ONESWEEP_COUNTER_SHIFT);
                    }
                    look_back_complete = true;
                }
                else
                {
                    look_back_reduction += gs_data[thread_index+ONESWEEP_DIGIT_RADIX];
                }
            }
            
            spin_count = 0;
        }
        else
        {
            if(!look_back_complete)
            {
                look_back_complete += counter_status >> ONESWEEP_COUNTER_SHIFT;
                if ((counter_status & ONESWEEP_STATUS_MASK) == ONESWEEP_STATUS_G)
                {
                    if (part_index < thread_blocks)
                    {
                        InterlockedAdd(pass_hist[])
                    }
                    look_back_complete = true;
                }
                else
                {
                    spin_count = 0; //wait re-lookbacking
                }
            }
        }
        
        look_back_index -= ONESWEEP_DIGIT_RADIX; //previous histotram index thread_index
        
        if(!warp_look_back_complete)
        {
            warp_look_back_complete = WaveActiveAllTrue(look_back_complete);
            if (warp_look_back_complete && WaveIsFirstLane())
                InterlockedAdd(gs_data[0], 1);
            GroupMemoryBarrierWithGroupSync();
        }
    }
    
    //post result write to shared memory
    if (thread_index < ONESWEEP_DIGIT_RADIX)
    {
        gs_data[thread_index + ] = look_back_reduction - exclusive_hist_reduction;
    }
    
}

inline void UpdateOffsetsWarpGE16(uint thread_index, inout LocalOffsets offsets, LocalKeys keys)
{
    if (thread_index > WaveGetLaneCount())
    {
        const uint t = GetWaveIndex(thread_index) * ONESWEEP_DIGIT_RADIX;
        UNROLL
        for (uint i = 0; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i)
        {
            const uint t2 = ExtractDigit(keys.keys[i], radix_shift);
            offsets.offsets[i] += gs_data[t2 + t] + gs_data[t2];
        }
    }
    else
    {
        UNROLL
        for (uint i = 0; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i)
            offsets.offsets[i] += gs_data[ExtractDigit(keys.keys[i], radix_shift)];

    }

}

inline void UpdateOffsetsWarpLT16(uint thread_index, uint iterations, inout LocalOffsets offsets, LocalKeys keys)
{
    if (thread_index > WaveGetLaneCount() * iterations)
    {
        const uint t = GetWaveIndex(thread_index) / iterations * ONESWEEP_DIGIT_HALF_RADIX;
        UNROLL
        for (uint i = 0; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i)
        {
            const uint t2 = ExtractPackedIndex(keys.keys[i], radix_shift);
            offsets.offsets[i] += ExtractPackedValue(gs_data[t2 + t] + gs_data[t2], keys, keys[i], radix_shift);
        }

    }
    else
    {
        UNROLL
        for (uint i = 0; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i)
            offsets.offsets[i] += ExtractPackedValue(gs_data[ExtractPackedIndex(keys.keys[i], radix_shift)], keys.keys[i], radix_shift);
    }
}


//if wave is too small, do not have enough space in shared memmory
//so instead, serialize it while each time 32 items
inline uint GetSerializeIeterations()
{
    return (ONESWEEP_DIGITBIN_DIM/WaveGetLaneCount() + 31) >> 5;
}

/**
* on a platform with small wave size, be care of shared memory size used
* if a wave is too small, some operation should be serialized for shared 
* memory space should be not enough
*/
inline uint WaveHistsSizeWarpGE16()
{
    return ONESWEEP_DIGITBIN_DIM / WaveGetLaneCount() * ONESWEEP_DIGIT_RADIX;
}

//for small wave usage whole shared data for hists
inline uint WaveHistsSizeWarpLT16()
{
    return ONESWEEP_SMEM_SIZE;
}

inline uint WaveFlags()
{
    return (WaveGetLaneCount() & 31) ? (1 << WaveGetLaneCount()) - 1 : 0xffffffffu;
}

inline void CountPeerBits(inout uint peer_bits, inout uint total_bits, uint4 wave_flags, uint wave_parts)
{
    for (uint wave_part = 0; wave_part < wave_parts; ++wave_part)
    {
        if (WaveGetLaneIndex() >= wave_part * ONESWEEP_WFLAGS_BITS)
        {
            const uint lt_mask = WaveGetLaneIndex() > (wave_part + 1) * ONESWEEP_WFLAGS_BITS ?
                                    0xffffffffu : (1u << (WaveGetLaneIndex() & ONESWEEP_WFLAGS_MASK)) - 1u;
            peer_bits += countbits(wave_flags[wave_part]); //bits index < lane_index
        }
        total_bits += countbits(wave_flags[wave_part]);  //all bits
    }
}

inline uint CountPeerBitsWarpLT16(uint wave_flags, uint lt_mask)
{
    return countbits(wave_flags & lt_mask);
}

inline uint FindLowestRankPeer(uint4 wave_flags, uint wave_parts)
{
    uint lowest_rank_peer = 0u;
    for (uint wave_part = 0; wave_part < wave_parts; ++wave_part)
    {
        uint fbl = firstbitlow(wave_flags[wave_parts]);
        if(fbl == 0xffffffffu)
            lowest_rank_peer += 32;
        else
            return lowest_rank_peer + fbl;
    }
    return 0;
}

inline void ClearWaveHists(uint thread_index)
{
    const uint hists_end = WaveGetLaneCount() >= 16 ? WaveHistsSizeWarpGE16() : WaveHistsSizeWarpLT16();
    for (uint i = thread_index; i < hists_end; i += ONESWEEP_DIGITBIN_DIM)
    {
        gs_data[i] = 0u;
    }
}

inline void LoadKeysWarpGE16(uint thread_index, uint partition_index, inout LocalKeys keys)
{
    UNROLL
    for(uint i = 0, t = 0;t < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += WaveGetLaneCount())
    {
        LoadKey(keys.keys[i], t);
    }
}

inline void LoadKeysTailWarpGE16(uint thread_index, uint partition_index, inout LocalKeys keys)
{
    UNROLL
    for (uint i = 0, t = 0; t < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += WaveGetLaneCount())
    {
        if (t < num_keys)
        {
            LoadKey(keys.keys[i], t);
        }
        else
        {
            LoadDummyKey(keys.keys[i]);
        }
    }
}

inline void LoadKeysWarpLT16(uint thread_index, uint partition_index, uint iterations, inout LocalKeys keys)
{
    UNROLL
    for (uint i = 0, t = 0; t < ONESWEEP_DIGITBIN_ITEM_COUNT;)
    {
        LoadKey(keys.keys[i], t);
    }

}

inline void LoadKeysTailWarpLT16(uint thread_index, uint partition_index, uint iterations, inout LocalKeys keys)
{
    UNROLL
    for (uint i = 0, t = GetThreadKeysDeviceOffsetWarpLT16(thread_index, partition_index, iterations);
        i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += WaveGetLaneIndex() * iterations)
    {
        if(t < num_keys)
            LoadKey(keys.keys[i], t);
        else
            LoadDummyKey(keys.keys[i]);
    }
}

inline void LoadPayloadsWarpGE16(uint thread_index, uint partition_index, inout LocalKeys payloads)
{
    UNROLL
    for (uint i = 0, t = GetThreadKeysDeviceOffsetWarpGE16(thread_index, partition_index);
                i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += WaveGetLaneCount())
    {
        LoadPayload(payloads.keys[i], t);
    }
}

inline void LoadPayloadsTailWarpGE16(uint thread_index, uint partion_index, inout LocalKeys payloads)
{
    UNROLL
    for (uint i = 0, t = GetThreadKeysDeviceOffsetWarpGE16(thread_index, partition_index);
                i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += WaveGetLaneCount())
    {
        if(t < num_keys)
            LoadPayload(payloads.keys[i], t);
    }
}

inline void LoadPayloadsWarpLT16(uint thread_index, uint partition_index, uint iterations, inout LocalKeys payloads)
{
    UNROLL
    for (uint i = 0, t = GetThreadKeysDeviceOffsetWarpLT16(thread_index, partition_index, iterations);
                    i < ONESWEEP_DIGITBIN_ITEM_COUNT; t += WaveGetLaneCount()*iterations)
    {
        LoadPayload(payloads.keys[i], t);
    }
}

inline void LoadPayloadsTailWarpLT16(uint thread_index, uint partition_index, uint iterations, inout LocalKeys payloads)
{
    UNROLL
    for (uint i = 0, t = GetThreadKeysDeviceOffsetWarpLT16(thread_index, partition_index, iterations);
                    i < ONESWEEP_DIGITBIN_ITEM_COUNT; t += WaveGetLaneCount() * iterations)
    {
        if(t < num_keys)
            LoadPayload(payloads.keys[i], t);
    }
}

/**
* warp-level histogram computation, for detals see "GPU Multisplit: an extended study of a parallel algorithm"
*/
inline void WaveLevelMultiSplitWarpGE16(uint key, uint wave_parts, inout uint4 wave_flags)
{
    UNROLL
    for (uint k = 0; k < ONESWEEP_DIGIT_LOG2; ++k)
    {
        const bool t = key >> (k + radix_shift) & 0x01; //grab k-th bit in radix
        const uint4 ballot = WaveActiveBallot(t);
        for (uint wave_part = 0; wave_part < wave_parts; ++wave_part)
            wave_flags[wave_part] &= (t ? 0 : 0xffffffffu) ^ ballot[wave_part];
    }

}

inline void WaveLevelMultiSplitWarpLT16(uint key, inout uint wave_flags)
{
    UNROLL
    for (uint k = 0; k < ONESWEEP_DIGIT_LOG2; ++k)
    {
        const bool t = key >> (k + radix_shift) & 1;
        wave_flags &= (t ? 0 : 0xffffffffu) ^ (uint) WaveActiveBallot(t);
    }

}

inline uint WaveHistInclusiveScanCircularShiftWarpGE16(uint thread_index)
{
    uint hist_reduction = gs_data[thread_index];
    for (uint i = thread_index + ONESWEEP_DIGIT_RADIX; i < WaveHistsSizeWarpGE16(); i += ONESWEEP_DIGIT_RADIX)
    {
        hist_reduction += gs_data[i];
        gs_data[i] = hist_reduction - gs_data[i]; //save exclusive
    }
    return hist_reduction; //inclusive 
}

inline void WaveHistReductionExclusiveScanWarpGE16(uint thread_index, inout uint hist_reduction)
{
    if(thread_index < ONESWEEP_DIGIT_RADIX)
    {
        const uint lt_mask = WaveGetLaneCount() - 1;
        //thread & ~ltmask
        gs_data[((WaveGetLaneIndex() + 1) & lt_mask) + (thread_index & ~lt_mask)] = hist_reduction;
    }
    GroupMemoryBarrierWithGroupSync();
    if (thread_index < ONESWEEP_DIGIT_RADIX / WaveGetLaneCount())
    {
        gs_data[thread_index * WaveGetLaneCount()] = WavePrefixSum(gs_data[thread_index * WaveGetLaneCount()]);
    }
    GroupMemoryBarrierWithGroupSync();
    if (thread_index < ONESWEEP_DIGIT_RADIX && WaveGetLaneIndex())
        gs_data[thread_index] += WaveReadLaneAt(gs_data[thread_index - 1], 1);
}


inline uint WaveHistInclusiveScanCircularShiftWarpLT16(uint thread_index)
{
    uint hist_reduction = gs_data[thread_index];
    for (uint i = thread_index + 9; i < WaveHistsSizeWarpLT16(); i += 9)
    {
        hist_reduction += gs_data[i];
        gs_data[i] = hist_reduction - gs_data[i];
    }
    return hist_reduction;
}

//blelloch scan for in place packed exclusive 
//https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-39-parallel-prefix-sum-scan-cuda
inline void WaveHistReductionExclusiveScanWarpLT16(uint thread_index)
{
    uint shift = 1;
    //upsweep
    for (uint j = ONESWEEP_DIGIT_RADIX >> 2; j > 0; j >>= 1)
    {
        GroupMemoryBarrierWithGroupSync();
        if(thread_index < j)
        {
            gs_data[((((thread_index << 1) + 2) << shift) - 1) >> 1] +=
                gs_data[((((thread_index << 1) + 1) << shift) - 1) >> 1] & 0xffff0000;
        }
        shift++;

    }
    GroupMemoryBarrierWithGroupSync();
    if(thread_index == 0)
        gs_data[ONESWEEP_DIGIT_HALF_RADIX - 1] &= 0xffffu;
    
    //downsweep
    for (uint j = 1; j < ONESWEEP_DIGIT_RADIX >> 1; j <<= 1)
    {
        --shift;
        GroupMemoryBarrierWithGroupSync();
        if(thread_index < j)
        {
            const uint t = ((((thread_index << 1) + 1) << shift) - 1) >> 1;
            const uint t2 = ((((thread_index << 1) + 2) << shift) - 1) >> 1;
            const uint t3 = gs_data[t];
            gs_data[t] = (gs_data[t] & 0xffffu) | (gs_data[t2] & 0xffff0000u);
            gs_data[t2] += t3 & 0xffff0000u;
        }
    }
    
    GroupMemoryBarrierWithGroupSync();
    if (thread_index < ONESWEEP_DIGIT_HALF_RADIX)
    {
        const uint t = gs_data[thread_index];
        gs_data[thread_index] = (t >> 16) + (t << 16) + (t & 0xffff0000u);
    }

}

inline void RankKeysWarpGE16(uint thread_index, LocalKeys keys, inout LocalOffsets offsets)
{
    const uint wave_parts = (WaveGetLaneCount() + 31) / 32;
    UNROLL
    for (uint i = 0; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i)
    {
        uint4 wave_flags = WaveFlags();
        WaveLevelMultiSplitWarpGE16(keys.keys[i], wave_parts, wave_flags); //all wave lane key == keys.keys[i]
        
        const uint index = ExtractDigit(keys.keys[i], radix_shift) + (GetWaveIndex(thread_index) * ONESWEEP_DIGIT_RADIX);//one warp one histogram
        const uint lowest_rank_peer = FindLowestRankPeer(wave_flags, wave_parts);
        
        uint peer_bits = 0;
        uint total_bits = 0;
        CountPeerBits(peer_bits, total_bits, wave_flags, wave_parts);
        
        uint pre_increment_val;
        if(!peer_bits)
            InterlockedAdd(gs_data[index], total_bits, pre_increment_val);
        offsets.offsets[i] = WaveReadLaneAt(pre_increment_val, lowest_rank_peer) + peer_bits;
    }

}

inline void RankKeysWarpLT16(uint thread_index, uint iterations, LocalKeys keys, inout LocalOffsets offsets)
{
    const uint lt_mask = (1u << WaveGetLaneIndex()) - 1;
    
    UNROLL
    for (uint i = 0; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i)
    {
        uint wave_flags = WaveFlags();
        WaveLevelMultiSplitWarpLT16(keys.keys[i], wave_flags);
        
        const uint index = ExtractPackedIndex(keys.keys[i], radix_shift) + (GetWaveIndex(thread_index) / iterations * ONESWEEP_DIGIT_HALF_RADIX);
        const uint peer_bits = CountPeerBitsWarpLT16(wave_flags, lt_mask);
        
        for (uint k = 0; k < iterations; ++k)
        {
            if(GetWaveIndex(thread_index) % iterations == k)
                offsets.offsets[i] = ExtractPackedValue(gs_data[index], keys.keys[i], radix_shift) + peer_bits;
            GroupMemoryBarrierWithGroupSync();
            if(GetWaveIndex(thread_index) % iterations == k && !peer_bits)
            {
                InterlockedAdd(gs_data[index], countbits(wave_flags) << ExtractPackedShift(keys.keys[i]));
            }
            GroupMemoryBarrierWithGroupSync();
        }
    }
}

inline void DeviceScatterKeysOnly(uint thread_index)
{
    for (uint i = thread_index; i < ONESWEEP_PART_SIZE; i += ONESWEEP_DIGITBIN_DIM)
        StoreKey(gs_data[ExtractDigit(gs_data[i], radix_shift) + ONESWEEP_PART_SIZE], i);
}

inline void DeviceScatterKeysOnlyTail(uint thread_index, uint final_part_size)
{
    for (uint i = thread_index; i < ONESWEEP_PART_SIZE; i += ONESWEEP_DIGITBIN_DIM)
    {
        if(i < final_part_size)
            StoreKey(gs_data[ExtractDigit(gs_data[i], radix_shift) + ONESWEEP_PART_SIZE], i);
    }
}

inline void DeviceScatterPairsKeyPhase(uint thread_index, inout LocalDigits digits)
{
    UNROLL
    for (uint i = 0, t = thread_index; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += ONESWEEP_DIGITBIN_DIM)
    {
        digits.digits[i] = ExtractDigit(gs_data[t], radix_shift);
        StoreKey(gs_data[digits.digits[i] + ONESWEEP_PART_SIZE] + t, t);
    }
}

inline void DeviceScatterPairsKeyPhaseTail(uint thread_index, uint final_part_size, inout LocalDigits digits)
{
    UNROLL
    for (uint i = 0, t = thread_index; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += ONESWEEP_DIGITBIN_DIM)
    {
        if(t < final_part_size)
        {
            digits.digits[i] = ExtractDigit(gs_data[t], radix_shift);
            StoreKey(gs_data[digits.digits[i] + ONESWEEP_PART_SIZE] + t, t);
        }
    }

}

inline void DeviceScatterPayloads(uint thread_index, LocalDigits digits)
{
    UNROLL
    for (uint i = 0, t = thread_index; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += ONESWEEP_DIGITBIN_DIM)
    {
        StorePayload(gs_data[digits.digits[i] + ONESWEEP_PART_SIZE] + t, t);
    }   
}

inline void DeviceScatterPayloadsTail(uint thread_index, uint final_part_size, LocalDigits digits)
{
    UNROLL
    for (uint i = 0, t = thread_index; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += ONESWEEP_DIGITBIN_DIM)
    {
        if (t < final_part_size)
            StorePayload(gs_data[digits.digits[i] + ONESWEEP_PART_SIZE] + t, t);
    }
}

inline void DeviceScatterPairs(uint thread_index, uint partition_index, LocalOffsets offsets)
{
    LocalDigits digits;
    DeviceScatterPairsKeyPhase(thread_index, digits);
    GroupMemoryBarrierWithGroupSync();
    
    LocalKeys payloads;
    if (WaveGetLaneCount() >= 16)
        LoadPayloadsWarpGE16(thread_index, partition_index, payloads);
    else
        LoadPayloadsWarpLT16(thread_index, partition_index, GetSerializeIeterations(), payloads);
    //
    GroupMemoryBarrierWithGroupSync();
    
    DeviceScatterPayloads(thread_index, digits);
}

inline void DeviceScatterPairsTail(uint thread_index, uint partition_index, uint final_part_size, LocalOffsets offsets)
{
    LocalDigits digits;
    const uint final_part_size = num_keys - partition_index * ONESWEEP_PART_SIZE;
    DeviceScatterPairsKeyPhaseTail(thread_index, final_part_size, digits);
    GroupMemoryBarrierWithGroupSync();
    
    LocalKeys payloads;
    if (WaveGetLaneCount() >= 16)
        LoadPayloadsTailWarpGE16(thread_index, partition_index, payloads);
    else
        LoadPayloadsTailWarpLT16(thread_index, partition_index, GetSerializeIeterations(), payloads);
    
    GroupMemoryBarrierWithGroupSync();
    
    DeviceScatterPayloadsTail(thread_index, final_part_size, digits);

}

inline void BlockScatterKeys(LocalOffsets offsets, LocalKeys keys)
{
    UNROLL
    for (uint i = 0; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i)
        gs_data[offsets.offsets[i]] = keys.keys[i];
}

inline void BlockScatterPayload(LocalOffsets offsets, LocalKeys payloads)
{
    BlockScatterKeys(offsets, payloads);
}


void DeviceScatter(uint thread_index, uint partition_index, LocalOffsets offsets)
{
#if defined(ONESWEEP_SORT_PAIRS)
    DeviceScatterPairs(thread_index, partition_index, offsets);
#else
    DeviceScatterKeysOnly(thread_index);
#endif
}

void DeviceScatterTail(uint thread_index, uint partition_index, LocalOffsets offsets)
{
    const uint final_part_size = num_keys - partition_index * ONESWEEP_PART_SIZE;
#if defined(ONESWEEP_SORT_PAIRS)
    DeviceScatterPairsTail(thread_index, final_part_size, offsets);
#else
    DeviceScatterKeysOnlyTail(thread_index, final_part_size);
#endif
}

[numthreads(ONESWEEP_DIGITBIN_DIM, 1, 1)]
void CSChainedScanDigitBinning(uint thread_index : SV_GroupIndex)
{
    uint partition_index;
    
    //using unwarp index
    //thread_index = GetUnwarpedThreadIndex(thread_index, ONESWEEP_DIGITBIN_DIM, WaveGetLaneCount());
    LocalKeys keys;
    LocalOffsets offsets;
    
    /*clear gs_data shared memory, if part size is bigger than wave size, need barrier*/
    if (WaveGetLaneCount() > 16)
    {
        if (WaveHistsSizeWarpGE16() < ONESWEEP_PART_SIZE)
            ClearWaveHists(thread_index);
        partition_index = GetPartitionTileIndex(thread_index);
        
        if (WaveHistsSizeWarpGE16() >= ONESWEEP_PART_SIZE)
        {
            GroupMemoryBarrierWithGroupSync();
            ClearWaveHists(thread_index);
            GroupMemoryBarrierWithGroupSync();
        }
    }
    
    if (WaveGetLaneCount() <= 16)
    {
        partition_index = GetPartitionTileIndex(thread_index);
        GroupMemoryBarrierWithGroupSync();
        ClearWaveHists(thread_index);
        GroupMemoryBarrierWithGroupSync();
    }
    
    //load key in, do serialize load while warp is bigger than 16 
    if(partition_index < thread_blocks - 1)
    {
        if (WaveGetLaneCount() >= 16)
        {
            LoadKeysWarpGE16(thread_index, partition_index, keys);
        }
        
        if (WaveGetLaneCount() < 16)
        {
            LoadKeysWarpLT16(thread_index, partition_index, GetSerializeIeterations(), keys);
        }

    }
    
    if(partition_index == thread_blocks - 1)
    {
        if (WaveGetLaneCount() >= 16)
        {
            LoadKeysTailWarpGE16(thread_index, partition_index, keys);
        }
        
        if (WaveGetLaneCount() < 16)
        {
            LoadKeysTailWarpLT16(thread_index, partition_index, GetSerializeIeterations(), keys);
        }

    }
    
    uint exclusive_hist_reduction;
    if (WaveGetLaneCount() >= 16)
    {
        RankKeysWarpGE16(thread_index, offsets, keys);
        GroupMemoryBarrierWithGroupSync();
        
        uint hist_reduction;
        if(thread_index < ONESWEEP_DIGIT_RADIX)
        {
            hist_reduction = WaveHistInclusiveScanCircularShiftWarpGE16(thread_index);
            DeviceBroadcastReductionsWarpGE16(thread_index, partition_index, hist_reduction);
            hist_reduction += WavePrefixSum(hist_reduction);
        }
        GroupMemoryBarrierWithGroupSync();
        
        WaveHistReductionExclusiveScanWarpGE16(thread_index, hist_reduction);
        GroupMemoryBarrierWithGroupSync();
        
        UpdateOffsetsWarpGE16(thread_index, offsets, keys);
        if(thread_index < ONESWEEP_DIGIT_RADIX)
            exclusive_hist_reduction = gs_data[thread_index];
        GroupMemoryBarrierWithGroupSync();

    }
    
    if (WaveGetLaneCount() < 16)
    {
        RankKeysWarpLT16(thread_index, GetSerializeIeterations(), keys, offsets);
        
        if (thread_index < ONESWEEP_DIGIT_HALF_RADIX)
        {
            uint hist_reduction = WaveHistInclusiveScanCircularShiftWarpLT16(thread_index);
            gs_data[thread_index] = hist_reduction + (hist_reduction << 16);
            DeviceBroadcastReductionWarpLT16(thread_index, partition_index, hist_reduction);
        }
        
        WaveHistReductionExclusiveScanWarpLT16(thread_index);
        GroupMemoryBarrierWithGroupSync();
        
        UpdateOffsetsWarpLT16(thread_index, GetSerializeIeterations(), offsets, keys);
        if(thread_index < ONESWEEP_DIGIT_RADIX)
            exclusive_hist_reduction = gs_data[thread_index >> 1] >> ((thread_index & 0x1) ? 16 : 0) & 0xffff; //decomposite packed data
        GroupMemoryBarrierWithGroupSync();
    }
    
    BlockScatterKeys(offsets, keys);
    //reduction
    LookBack(thread_index, partition_index, exclusive_hist_reduction);
    GroupMemoryBarrierWithGroupSync();
    
    //scatter
    if(partition_index < thread_blocks - 1)
        DeviceScatter(thread_index, partition_index, offsets);
    
    if(partition_index == thread_blocks - 1)
        DeviceScatterTail(thread_index, partition_index, offsets);

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
//(FXRively a negation) or leave the bits alone.  When the the NullItem is
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
    UNROLL
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

    UNROLL
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