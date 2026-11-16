#ifndef _PARALLEL_SORT_INC_
#define _PARALLEL_SORT_INC_

#include "Platform.hlsli"
#include "BindlessResources.hlsli"

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
    uint mask = ((u >> 31) - 1) | 0x80000000u;
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
#define ONESWEEP_PART_SIZE 1792u
#define ONESWEEP_DIGITBIN_ITEM_COUNT (ONESWEEP_PART_SIZE / ONESWEEP_DIGITBIN_DIM)

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
#define ONESWEEP_STATUS_MASK 0x00000003u
#define ONESWEEP_COUNTER_MASK ~ONESWEEP_STATUS_MASK
#define ONESWEEP_STATUS_SHIFT 0
#define ONESWEEP_COUNTER_SHIFT 2
#define ONESWEEP_STATUS_N 0 //not ready
#define ONESWEEP_STATUS_L 1 //local count
#define ONESWEEP_STATUS_G 2 //global sum
#define ONESWEEP_STATUS_R 3 //not used
#endif


/**
* shared memory size for digit binning, smem[ONESWEEP_SMEM_SIZE-1] to store partition_index
* smem[0] to store
*/
#define ONESWEEP_SMEM_SIZE 4096u

//wave flags bits and mask
#define ONESWEEP_WFLAGS_BITS 32u //sizeof(uint)
#define ONESWEEP_WFLAGS_MASK (ONESWEEP_WFLAGS_BITS-1)


cbuffer onesweep_config
{
    uint num_keys;
    uint radix_shift;
    uint thread_blocks;
    uint global_hist_blocks;
    uint is_tail;
    uint is_reverse;
    uint global_hist_rw_bf_index;
    uint pass_hist_ch_rw_bf_index;
    uint sort_key_rw_bf_index;
    uint alt_key_rw_bf_index;

    uint sort_payload_rw_bf_index;
    uint alt_payload_rw_bf_index;

    uint counter_ch_rw_bf_index;
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

inline uint GetBlockWavePass()
{
    return ONESWEEP_DIGITBIN_DIM / WaveGetLaneCount();
}

inline uint GetWaveIndex(uint thread_index)
{
    return thread_index / THREAD_WARP_SIZE;
}

inline uint PassHistOffset(uint partition_index)
{
    return ((radix_shift / ONESWEEP_DIGIT_LOG2) * thread_blocks + partition_index) * ONESWEEP_DIGIT_RADIX;
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
    const uint partition_end = (group_index == global_hist_blocks - 1) ? num_keys : (group_index + 1) * ONESWEEP_UPFRONT_TILE_SIZE;
    
    uint t;
    for (uint i = thread_index + group_index * ONESWEEP_UPFRONT_TILE_SIZE; i < partition_end; i += ONESWEEP_UPFRONT_DIM) //deal with data tile
    {
        //load data to hist from global memory
#if defined(ONESWEEP_KEY_UINT)
            t = GetBindlessRWByteBufferUniform(sort_key_rw_bf_index).Load(i * 4u);
#elif defined(ONESWEEP_KEY_INT)
            t = ConvertIntToUint(asint(GetBindlessRWByteBufferUniform(sort_key_rw_bf_index).Load(i * 4u)));
#elif defined(ONESWEEP_KEY_FLOAT)
            t = ConvertFloatToUint(asfloat(GetBindlessRWByteBufferUniform(sort_key_rw_bf_index).Load(i * 4u)));
#endif 
        t = CondFlipBitsUint(t, is_reverse);
        InterlockedAdd(gs_global_hist[hist_offset + ExtractDigit(t, 0)].x, 1);
        InterlockedAdd(gs_global_hist[hist_offset + ExtractDigit(t, 8)].y, 1);
        InterlockedAdd(gs_global_hist[hist_offset + ExtractDigit(t, 16)].z, 1);
        InterlockedAdd(gs_global_hist[hist_offset + ExtractDigit(t, 24)].w, 1);
    }

}

//write local histogram to global memory
void GlobalHistogramReduceWrite(uint thread_index)
{
    RWByteAddressBuffer global_hist = GetBindlessRWByteBufferUniform(global_hist_rw_bf_index);
    for (uint i = thread_index; i < ONESWEEP_DIGIT_RADIX; i += ONESWEEP_UPFRONT_DIM)
    {
        uint original;
        global_hist.InterlockedAdd(i * 4u, gs_global_hist[i].x + gs_global_hist[i + ONESWEEP_DIGIT_RADIX].x, original);
        global_hist.InterlockedAdd((i + ONESWEEP_SEC_RADIX_OFFSET) * 4u, gs_global_hist[i].y + gs_global_hist[i + ONESWEEP_DIGIT_RADIX].y, original);
        global_hist.InterlockedAdd((i + ONESWEEP_THIRD_RADIX_OFFSET) * 4u, gs_global_hist[i].z + gs_global_hist[i + ONESWEEP_DIGIT_RADIX].z, original);
        global_hist.InterlockedAdd((i + ONESWEEP_FOURTH_RADIX_OFFSET) * 4u, gs_global_hist[i].w + gs_global_hist[i + ONESWEEP_DIGIT_RADIX].w, original);
    }
}

//load ans scan in one warp
inline void LoadInclusiveScan(uint group_index, uint thread_index)
{
    RWByteAddressBuffer global_hist = GetBindlessRWByteBufferUniform(global_hist_rw_bf_index);
    const uint t = global_hist.Load((thread_index + group_index * ONESWEEP_DIGIT_RADIX) * 4u);
    gs_scan[thread_index] = t + WavePrefixSum(t);
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
    GetCoherentBindlessRWByteBuffer(pass_hist_ch_rw_bf_index).Store(
        (index + group_index * ONESWEEP_DIGIT_RADIX * thread_blocks) * 4u,
        (((WaveGetLaneIndex() != lane_mask ? gs_scan[thread_index] : 0) +
            (thread_index >= WaveGetLaneCount() ? WaveReadLaneAt(gs_scan[thread_index - 1], 0) : 0)) << ONESWEEP_COUNTER_SHIFT) | ONESWEEP_STATUS_G);

}

inline void GlobalExclusiveSumKernelWarpLT16(uint group_index, uint thread_index)
{
    RWByteAddressBuffer pass_hist = GetCoherentBindlessRWByteBuffer(pass_hist_ch_rw_bf_index);
    const uint pass_hist_offset = group_index * ONESWEEP_EXCSUM_DIM * thread_blocks;
    if (thread_index < WaveGetLaneCount())
    {
        const uint circular_lane_shift = WaveGetLaneIndex() + 1 & WaveGetLaneCount() - 1;
        pass_hist.Store((circular_lane_shift + pass_hist_offset) * 4u,
            ((circular_lane_shift ? gs_scan[thread_index] : 0) << ONESWEEP_COUNTER_SHIFT) | ONESWEEP_STATUS_G);
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
                pass_hist.Store((thread_index + pass_hist_offset) * 4u, ((WaveReadLaneAt(gs_scan[((thread_index >> offset) << offset) - 1], 0) +
                    ((thread_index & (j - 1)) ? gs_scan[thread_index - 1] : 0)) << ONESWEEP_COUNTER_SHIFT) | ONESWEEP_STATUS_G);
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
        pass_hist.Store((index + pass_hist_offset) * 4u, ((WaveReadLaneAt(gs_scan[((index >> offset) << offset) - 1], 0) +
            ((index & (j - 1)) ? gs_scan[index - 1] : 0)) << ONESWEEP_COUNTER_SHIFT) | ONESWEEP_STATUS_G);
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
    {
        uint original;
        GetCoherentBindlessRWByteBuffer(counter_ch_rw_bf_index).InterlockedAdd((radix_shift / ONESWEEP_DIGIT_LOG2) * 4u, 1u, gs_data[ONESWEEP_SMEM_SIZE - 1]);
    }
    GroupMemoryBarrierWithGroupSync();
    return gs_data[ONESWEEP_SMEM_SIZE - 1];
}

inline uint GetCurrentPassIndex()
{
    return radix_shift / ONESWEEP_DIGIT_LOG2;
}

inline uint GetPartitionPassHistOffset(uint partition_index)
{
    return (GetCurrentPassIndex() * thread_blocks + partition_index) * ONESWEEP_DIGIT_RADIX;
}

inline uint GetGlobalPassHistOffset()
{
    return radix_shift * 32u;
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
    return GetThreadKeysSharedMemoryOffsetWarpLT16(thread_index, iterations) + partition_index * ONESWEEP_PART_SIZE;
}

inline void LoadKey(inout uint key, uint device_index)
{
    uint t = GetBindlessRWByteBufferUniform(sort_key_rw_bf_index).Load(device_index * 4u);
#if defined(ONESWEEP_KEY_INT)
    t = ConvertIntToUint(asint(t));
#elif defined(ONESWEEP_KEY_FLOAT)
    t = ConvertFloatToUint(asfloat(t));
#endif 
    key = CondFlipBitsUint(t, is_reverse);
}

inline void LoadDummyKey(inout uint key)
{
    key = 0xffffffffu;
}

inline void StoreKeyValue(uint device_index, uint key)
{
    key = CondFlipBitsUint(key, is_reverse);
    RWByteAddressBuffer key_buffer = GetBindlessRWByteBufferUniform(alt_key_rw_bf_index);
#if defined(ONESWEEP_KEY_UINT)
    key_buffer.Store(device_index * 4u, key);
#elif defined(ONESWEEP_KEY_INT)
    key_buffer.Store(device_index * 4u, asuint(InvConvertUintToInt(key)));
#elif defined(ONESWEEP_KEY_FLOAT)
    key_buffer.Store(device_index * 4u, asuint(InvConvertUintToFloat(key)));
#endif 
}

inline void StoreKey(uint device_index, uint gs_index)
{
    StoreKeyValue(device_index, gs_data[gs_index]);
}

inline void LoadPayload(inout uint payload, uint device_index)
{
    payload = GetBindlessRWByteBufferUniform(sort_payload_rw_bf_index).Load(device_index * 4u);
}

inline void StorePayload(uint device_index, uint gs_index)
{
    RWByteAddressBuffer payload_buffer = GetBindlessRWByteBufferUniform(alt_payload_rw_bf_index);
#ifdef ONESWEEP_PAYLOAD_UINT
    payload_buffer.Store(device_index * 4u, gs_data[gs_index]);
#elif defined(ONESWEEP_PAYLOAD_INT)
    payload_buffer.Store(device_index * 4u, gs_data[gs_index]);
#elif defined(ONESWEEP_PAYLOAD_FLOAT)
    payload_buffer.Store(device_index * 4u, gs_data[gs_index]);
#endif
}

inline void DeviceBroadcastReductionsWarpGE16(uint thread_index, uint partition_Index, uint hist_reduction)
{
    RWByteAddressBuffer pass_hist = GetCoherentBindlessRWByteBuffer(pass_hist_ch_rw_bf_index);
    
    if (partition_Index < thread_blocks - 1)
    {
        uint original;
        pass_hist.InterlockedAdd((thread_index + PassHistOffset(partition_Index + 1)) * 4u, (hist_reduction << ONESWEEP_COUNTER_SHIFT) | ONESWEEP_STATUS_L, original);
    }
}

inline void DeviceBroadcastReductionWarpLT16(uint thread_index, uint partition_Index, uint hist_reduction)
{
    RWByteAddressBuffer pass_hist = GetCoherentBindlessRWByteBuffer(pass_hist_ch_rw_bf_index);
    
    if (partition_Index < thread_blocks - 1)
    {
        uint original;
        pass_hist.InterlockedAdd(((thread_index << 1) + PassHistOffset(partition_Index + 1)) * 4u,
            ((hist_reduction & 0xffffu) << ONESWEEP_COUNTER_SHIFT) | ONESWEEP_STATUS_L, original);
        pass_hist.InterlockedAdd(((thread_index << 1) + 1 + PassHistOffset(partition_Index + 1)) * 4u,
            (((hist_reduction >> 16) & 0xffffu) << ONESWEEP_COUNTER_SHIFT) | ONESWEEP_STATUS_L, original);
    }
}

//one thread to accumulate hist[thread_index] and lookback
inline void LookBack(uint thread_index, uint part_index, uint exclusive_hist_reduction)
{
    if (thread_index >= ONESWEEP_DIGIT_RADIX)
        return;

    RWByteAddressBuffer pass_hist = GetCoherentBindlessRWByteBuffer(pass_hist_ch_rw_bf_index);
    uint look_back_reduction = 0u;
    uint k = part_index;
    uint spin_count = 0u;

    while (true)
    {
        const uint counter_index = thread_index + GetPartitionPassHistOffset(k);
        const uint counter_status = pass_hist.Load(counter_index * 4u);
        const uint status = counter_status & ONESWEEP_STATUS_MASK;

        if (status == ONESWEEP_STATUS_G)
        {
            look_back_reduction += counter_status >> ONESWEEP_COUNTER_SHIFT;
            if (part_index < thread_blocks - 1)
            {
                //sync to global
                uint original;
                pass_hist.InterlockedAdd((thread_index + PassHistOffset(part_index + 1)) * 4u,
                    (look_back_reduction << ONESWEEP_COUNTER_SHIFT) | ONESWEEP_STATUS_L, original);
            }
            gs_data[thread_index + ONESWEEP_PART_SIZE] = look_back_reduction - exclusive_hist_reduction;
            break;
        }

        if (status == ONESWEEP_STATUS_L)
        {
            look_back_reduction += counter_status >> ONESWEEP_COUNTER_SHIFT;
            if (k == 0u)
            {
                gs_data[thread_index + ONESWEEP_PART_SIZE] = look_back_reduction - exclusive_hist_reduction;
                break;
            }
            --k;
            spin_count = 0u;
            continue;
        }

        if (spin_count < ONESWEEP_MAX_SPIN_COUNT)
        {
            ++spin_count;
            continue;
        }

        const uint part_begin = k * ONESWEEP_PART_SIZE;
        const uint part_end = min(part_begin + ONESWEEP_PART_SIZE, num_keys);
        uint fallback_count = 0u;
        for (uint i = part_begin; i < part_end; ++i)
        {
            uint key;
            LoadKey(key, i);
            fallback_count += ExtractDigit(key, radix_shift) == thread_index ? 1u : 0u;
        }

        uint original;
        pass_hist.InterlockedCompareExchange(counter_index * 4u, 0u,
            (fallback_count << ONESWEEP_COUNTER_SHIFT) | ONESWEEP_STATUS_L, original);

        const uint original_status = original & ONESWEEP_STATUS_MASK;
        look_back_reduction += original_status == ONESWEEP_STATUS_N ? fallback_count : original >> ONESWEEP_COUNTER_SHIFT;
        if (original_status == ONESWEEP_STATUS_G)
        {
            if (part_index < thread_blocks - 1)
            {
                uint propagated;
                pass_hist.InterlockedAdd((thread_index + PassHistOffset(part_index + 1)) * 4u,
                    (look_back_reduction << ONESWEEP_COUNTER_SHIFT) | ONESWEEP_STATUS_L, propagated);
            }
            gs_data[thread_index + ONESWEEP_PART_SIZE] = look_back_reduction - exclusive_hist_reduction;
            break;
        }
        if (k == 0u)
        {
            gs_data[thread_index + ONESWEEP_PART_SIZE] = look_back_reduction - exclusive_hist_reduction;
            break;
        }
        --k;
        spin_count = 0u;
    }
}

inline void UpdateOffsetsWarpGE16(uint thread_index, inout LocalOffsets offsets, LocalKeys keys)
{
    if (thread_index >= WaveGetLaneCount())
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
    const uint iteration_count = 1u;
    if (thread_index >= WaveGetLaneCount() * iteration_count)
    {
        const uint t = GetWaveIndex(thread_index) / iteration_count * ONESWEEP_DIGIT_HALF_RADIX;
        UNROLL
        for (uint i = 0; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i)
        {
            const uint t2 = ExtractPackedIndex(keys.keys[i], radix_shift);
            offsets.offsets[i] += ExtractPackedValue(gs_data[t2 + t] + gs_data[t2], keys.keys[i], radix_shift);
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
inline uint GetSerializeIterations()
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
            const uint lt_mask = WaveGetLaneIndex() >= (wave_part + 1) * ONESWEEP_WFLAGS_BITS ?
                                    0xffffffffu : (1u << (WaveGetLaneIndex() & ONESWEEP_WFLAGS_MASK)) - 1u;
            peer_bits += countbits(wave_flags[wave_part] & lt_mask);
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
        uint fbl = firstbitlow(wave_flags[wave_part]);
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
    for (uint i = 0, t = GetThreadKeysDeviceOffsetWarpGE16(thread_index, partition_index); i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += WaveGetLaneCount())
    {
        LoadKey(keys.keys[i], t);
    }
}

inline void LoadKeysTailWarpGE16(uint thread_index, uint partition_index, inout LocalKeys keys)
{
    UNROLL
    for (uint i = 0, t = GetThreadKeysDeviceOffsetWarpGE16(thread_index, partition_index); i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += WaveGetLaneCount())
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
    for (uint i = 0, t = GetThreadKeysDeviceOffsetWarpLT16(thread_index, partition_index, iterations);
        i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += WaveGetLaneCount() * iterations)
    {
        LoadKey(keys.keys[i], t);
    }
}

inline void LoadKeysTailWarpLT16(uint thread_index, uint partition_index, uint iterations, inout LocalKeys keys)
{
    UNROLL
    for (uint i = 0, t = GetThreadKeysDeviceOffsetWarpLT16(thread_index, partition_index, iterations);
        i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += WaveGetLaneCount() * iterations)
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

inline void LoadPayloadsTailWarpGE16(uint thread_index, uint partition_index, inout LocalKeys payloads)
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
        i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += WaveGetLaneCount() * iterations)
    {
        LoadPayload(payloads.keys[i], t);
    }
}

inline void LoadPayloadsTailWarpLT16(uint thread_index, uint partition_index, uint iterations, inout LocalKeys payloads)
{
    UNROLL
    for (uint i = 0, t = GetThreadKeysDeviceOffsetWarpLT16(thread_index, partition_index, iterations);
        i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += WaveGetLaneCount() * iterations)
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
        const uint hist_count = gs_data[i];
        hist_reduction += hist_count;
        gs_data[i] = hist_reduction - hist_count; //save exclusive
    }
    return hist_reduction; //inclusive 
}

inline void WaveHistReductionExclusiveScanWarpGE16(uint thread_index, inout uint hist_reduction)
{
    gs_data[thread_index] = hist_reduction;
    GroupMemoryBarrierWithGroupSync();

    for (uint offset = 1; offset < ONESWEEP_DIGIT_RADIX; offset <<= 1)
    {
        const uint value = thread_index >= offset ? gs_data[thread_index - offset] : 0u;
        GroupMemoryBarrierWithGroupSync();
        gs_data[thread_index] += value;
        GroupMemoryBarrierWithGroupSync();
    }

    const uint exclusive = thread_index ? gs_data[thread_index - 1] : 0u;
    GroupMemoryBarrierWithGroupSync();
    gs_data[thread_index] = exclusive;
    GroupMemoryBarrierWithGroupSync();
}


inline uint WaveHistInclusiveScanCircularShiftWarpLT16(uint thread_index)
{
    uint hist_reduction = gs_data[thread_index];
    for (uint i = thread_index + ONESWEEP_DIGIT_HALF_RADIX; i < WaveHistsSizeWarpLT16(); i += ONESWEEP_DIGIT_HALF_RADIX)
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
    const uint4 wave_flags_init = WaveFlags();
    UNROLL
    for (uint i = 0; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i)
    {
        uint4 wave_flags = wave_flags_init;
        WaveLevelMultiSplitWarpGE16(keys.keys[i], wave_parts, wave_flags); //all wave lane key == keys.keys[i]
        
        const uint index = ExtractDigit(keys.keys[i], radix_shift) + (GetWaveIndex(thread_index) * ONESWEEP_DIGIT_RADIX);//one warp one histogram
        const uint lowest_rank_peer = FindLowestRankPeer(wave_flags, wave_parts);
        
        uint peer_bits = 0;
        uint total_bits = 0;
        CountPeerBits(peer_bits, total_bits, wave_flags, wave_parts);
        
        uint pre_increment_val = 0u;
        if(!peer_bits)
            InterlockedAdd(gs_data[index], total_bits, pre_increment_val);
        offsets.offsets[i] = WaveReadLaneAt(pre_increment_val, lowest_rank_peer) + peer_bits;
    }

}

inline void RankKeysWarpLT16(uint thread_index, uint iterations, LocalKeys keys, inout LocalOffsets offsets)
{
    const uint iteration_count = 1u;
    const uint lt_mask = (1u << WaveGetLaneIndex()) - 1;
    
    UNROLL
    for (uint i = 0; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i)
    {
        uint wave_flags = WaveFlags();
        WaveLevelMultiSplitWarpLT16(keys.keys[i], wave_flags);
        
        const uint index = ExtractPackedIndex(keys.keys[i], radix_shift) + (GetWaveIndex(thread_index) / iteration_count * ONESWEEP_DIGIT_HALF_RADIX);
        const uint peer_bits = CountPeerBitsWarpLT16(wave_flags, lt_mask);
        
        for (uint k = 0; k < iteration_count; ++k)
        {
            if(GetWaveIndex(thread_index) % iteration_count == k)
                offsets.offsets[i] = ExtractPackedValue(gs_data[index], keys.keys[i], radix_shift) + peer_bits;
            GroupMemoryBarrierWithGroupSync();
            if(GetWaveIndex(thread_index) % iteration_count == k && !peer_bits)
            {
                InterlockedAdd(gs_data[index], countbits(wave_flags) << ExtractPackedShift(keys.keys[i], radix_shift));
            }
            GroupMemoryBarrierWithGroupSync();
        }
    }
}

inline void DeviceScatterKeysOnly(uint thread_index)
{
    for (uint i = thread_index; i < ONESWEEP_PART_SIZE; i += ONESWEEP_DIGITBIN_DIM)
    {
        const uint key = gs_data[i];
        StoreKeyValue(gs_data[ExtractDigit(key, radix_shift) + ONESWEEP_PART_SIZE] + i, key);
    }
}

inline void DeviceScatterKeysOnlyTail(uint thread_index, uint final_part_size)
{
    for (uint i = thread_index; i < ONESWEEP_PART_SIZE; i += ONESWEEP_DIGITBIN_DIM)
    {
        if(i < final_part_size)
        {
            const uint key = gs_data[i];
            StoreKeyValue(gs_data[ExtractDigit(key, radix_shift) + ONESWEEP_PART_SIZE] + i, key);
        }
    }
}

inline void DeviceScatterPairsKeyPhase(uint thread_index, inout LocalDigits digits)
{
    UNROLL
    for (uint i = 0, t = thread_index; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += ONESWEEP_DIGITBIN_DIM)
    {
        const uint key = gs_data[t];
        digits.digits[i] = ExtractDigit(key, radix_shift);
        StoreKeyValue(gs_data[digits.digits[i] + ONESWEEP_PART_SIZE] + t, key);
    }
}

inline void DeviceScatterPairsKeyPhaseTail(uint thread_index, uint final_part_size, inout LocalDigits digits)
{
    UNROLL
    for (uint i = 0, t = thread_index; i < ONESWEEP_DIGITBIN_ITEM_COUNT; ++i, t += ONESWEEP_DIGITBIN_DIM)
    {
        if(t < final_part_size)
        {
            const uint key = gs_data[t];
            digits.digits[i] = ExtractDigit(key, radix_shift);
            StoreKeyValue(gs_data[digits.digits[i] + ONESWEEP_PART_SIZE] + t, key);
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

inline void DeviceScatterPairs(uint thread_index, uint partition_index, LocalOffsets offsets)
{
    LocalDigits digits;
    DeviceScatterPairsKeyPhase(thread_index, digits);
    GroupMemoryBarrierWithGroupSync();
    
    LocalKeys payloads;
    if (WaveGetLaneCount() >= 16)
        LoadPayloadsWarpGE16(thread_index, partition_index, payloads);
    else
        LoadPayloadsWarpLT16(thread_index, partition_index, GetSerializeIterations(), payloads);
    BlockScatterPayload(offsets, payloads);
    GroupMemoryBarrierWithGroupSync();
    
    DeviceScatterPayloads(thread_index, digits);
}

inline void DeviceScatterPairsTail(uint thread_index, uint partition_index, uint final_part_size, LocalOffsets offsets)
{
    LocalDigits digits;
    DeviceScatterPairsKeyPhaseTail(thread_index, final_part_size, digits);
    GroupMemoryBarrierWithGroupSync();
    
    LocalKeys payloads;
    if (WaveGetLaneCount() >= 16)
        LoadPayloadsTailWarpGE16(thread_index, partition_index, payloads);
    else
        LoadPayloadsTailWarpLT16(thread_index, partition_index, GetSerializeIterations(), payloads);
    BlockScatterPayload(offsets, payloads);
    GroupMemoryBarrierWithGroupSync();
    
    DeviceScatterPayloadsTail(thread_index, final_part_size, digits);

}

void DeviceScatter(uint thread_index, uint partition_index, LocalOffsets offsets)
{
#if defined(USE_PAIR_ONESWEEP)
    DeviceScatterPairs(thread_index, partition_index, offsets);
#else
    DeviceScatterKeysOnly(thread_index);
#endif
}

void DeviceScatterTail(uint thread_index, uint partition_index, LocalOffsets offsets)
{
    const uint final_part_size = num_keys - partition_index * ONESWEEP_PART_SIZE;
#if defined(USE_PAIR_ONESWEEP)
    DeviceScatterPairsTail(thread_index, partition_index, final_part_size, offsets);
#else
    DeviceScatterKeysOnlyTail(thread_index, final_part_size);
#endif
}

//bitonic sort time complexity see https://en.wikipedia.org/wiki/Bitonic_sorter
//code merged from https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/Bitonic32PreSortCS.hlsl

#define BITONIC_INNER_METHOD 1 //load data to LDS and loop sort on GPU
#define BITONIC_OUTTER_METHOD 2 //sort directly in memory and sync/loop on cpu
//use outter loop for large array size bigger than 2048, while inner loop for others 
#ifndef USE_BITONIC_METHOD
#define USE_BITONIC_METHOD BITONIC_INNER_METHOD
#endif

/*
*32bit: key; 64bit: key-index pair
*/
#ifndef USE_64BIT_BITONICSORT
#define USE_64BIT_BITONICSORT 1
#endif
#define BITONMIC_ELEMENT_MAX_NUM  2048 //sort for up to 2048 values
#define BITONIC_THREAD_GROUP_SIZE BITONMIC_ELEMENT_MAX_NUM/2 //use 1024 for thread group size
#define BITONIC_LDS_DATA_COUNT BITONMIC_ELEMENT_MAX_NUM
#define BITONIC_LDS_INDEX_MASK (BITONIC_LDS_DATA_COUNT-1)

#if USE_64BIT_BITONICSORT
#define ELEMENT_TYPE uint2
#else
#define ELEMENT_TYPE uint
#endif
#if USE_64BIT_BITONICSORT
#define BITONIC_ELEMENT_SIZE 8u
#else
#define BITONIC_ELEMENT_SIZE 4u
#endif

//determines if two sort keys should be swapped in the list.  NullItem is
//either 0 or 0xffffffff.  XOR with the NullItem will either invert the bits
//(FXRively a negation) or leave the bits alone.  When the the NullItem is
//0, we are sorting descending, so when lhs < rhs, they should swap.  For an
//ascending sort, ~lhs < ~rhs should swap.
inline bool IsSwapNeeded(uint lhs, uint rhs, uint null_item)
{
    return (lhs ^ null_item) < (rhs ^ null_item);
}

inline bool IsSwapNeeded(uint2 lhs, uint2 rhs, uint2 null_item)
{
    return (lhs.y ^ null_item.y) < (rhs.y ^ null_item.y);
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

void LoadSortKey(RWByteAddressBuffer sort_data, uint element_index, uint total_element_count, uint null_item)
{
    if (element_index < total_element_count)
    {
#if !USE_64BIT_BITONICSORT
        gs_sort_keys[LDS_ELEMENT_INDEX(element_index)] = sort_data.Load(element_index * BITONIC_ELEMENT_SIZE);
#else
        uint2 key_index_pair = sort_data.Load2(element_index * BITONIC_ELEMENT_SIZE);
        gs_sort_keys[LDS_ELEMENT_INDEX(element_index)] = key_index_pair.y;
        gs_sort_indices[LDS_ELEMENT_INDEX(element_index)] = key_index_pair.x;
#endif

    }
    else
    {
#if !USE_64BIT_BITONICSORT
        gs_sort_keys[LDS_ELEMENT_INDEX(element_index)] = null_item;
#else
        gs_sort_keys[LDS_ELEMENT_INDEX(element_index)] = null_item;
        gs_sort_indices[LDS_ELEMENT_INDEX(element_index)] = 0u;
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
        sort_data.Store2(element_index * BITONIC_ELEMENT_SIZE,
            uint2(gs_sort_indices[LDS_ELEMENT_INDEX(element_index)], gs_sort_keys[LDS_ELEMENT_INDEX(element_index)]));
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
    
    uint null_item = reverse ? 0x00000000u : 0xffffffffu;
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
    uint null_item = reverse ? 0x00000000u : 0xffffffffu;
    //load memory data to LDS
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
    
    StoreSortKeyIndexPair(sort_data, group_offset + thread_index, list_element_count);
    StoreSortKeyIndexPair(sort_data, group_offset + thread_index + BITONIC_THREAD_GROUP_SIZE, list_element_count);
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
#define LoadElement(idx) sort_data.Load2((idx) * BITONIC_ELEMENT_SIZE)
#define StoreElement(idx, element) sort_data.Store2((idx) * BITONIC_ELEMENT_SIZE, element)
    //descending: 0x0000000u; ascending: 0xffffffffu
    ELEMENT_TYPE null_item = reverse ? uint2(0x00000000u, 0x00000000u) : uint2(0xffffffffu, 0xffffffffu);
#else
#define LoadElement(idx) sort_data.Load((idx) * BITONIC_ELEMENT_SIZE)
#define StoreElement(idx, element) sort_data.Store((idx) * BITONIC_ELEMENT_SIZE, element)
    ELEMENT_TYPE null_item = reverse ? 0x00000000u : 0xffffffffu;
#endif
    ELEMENT_TYPE lhs_val = LoadElement(index1);
    ELEMENT_TYPE rhs_val = LoadElement(index2);

    if(IsSwapNeeded(lhs_val, rhs_val, null_item))
    {
        StoreElement(index1, rhs_val);
        StoreElement(index2, lhs_val);
    }
}
#else
#error "not supported bitonic loop method"
#endif

#endif //_PARALLEL_SORT_INC_
