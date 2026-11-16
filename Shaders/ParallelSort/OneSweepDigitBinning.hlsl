#include "../ParallelSort.hlsli"

[numthreads(ONESWEEP_DIGITBIN_DIM, 1, 1)]
void CSChainedScanDigitBinning(uint thread_index : SV_GroupIndex)
{
    uint partition_index;

    LocalKeys keys = (LocalKeys)0;
    LocalOffsets offsets = (LocalOffsets)0;

    if (WaveGetLaneCount() > 16)
    {
        if (WaveHistsSizeWarpGE16() < ONESWEEP_PART_SIZE)
            ClearWaveHists(thread_index);
        partition_index = GetPartitionTileIndex(thread_index);

        if (WaveHistsSizeWarpGE16() >= ONESWEEP_PART_SIZE)
        {
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

    if (partition_index < thread_blocks - 1)
    {
        if (WaveGetLaneCount() >= 16)
        {
            LoadKeysWarpGE16(thread_index, partition_index, keys);
        }

#if THREAD_WARP_SIZE < 16
        if (WaveGetLaneCount() < 16)
        {
            LoadKeysWarpLT16(thread_index, partition_index, GetSerializeIterations(), keys);
        }
#endif
    }

    if (partition_index == thread_blocks - 1)
    {
        if (WaveGetLaneCount() >= 16)
        {
            LoadKeysTailWarpGE16(thread_index, partition_index, keys);
        }

#if THREAD_WARP_SIZE < 16
        if (WaveGetLaneCount() < 16)
        {
            LoadKeysTailWarpLT16(thread_index, partition_index, GetSerializeIterations(), keys);
        }
#endif
    }

    uint exclusive_hist_reduction = 0u;
    if (WaveGetLaneCount() >= 16)
    {
        RankKeysWarpGE16(thread_index, keys, offsets);
        GroupMemoryBarrierWithGroupSync();

        uint hist_reduction = 0u;
        if (thread_index < ONESWEEP_DIGIT_RADIX)
        {
            hist_reduction = WaveHistInclusiveScanCircularShiftWarpGE16(thread_index);
            DeviceBroadcastReductionsWarpGE16(thread_index, partition_index, hist_reduction);
        }
        GroupMemoryBarrierWithGroupSync();

        WaveHistReductionExclusiveScanWarpGE16(thread_index, hist_reduction);
        GroupMemoryBarrierWithGroupSync();

        UpdateOffsetsWarpGE16(thread_index, offsets, keys);
        if (thread_index < ONESWEEP_DIGIT_RADIX)
            exclusive_hist_reduction = gs_data[thread_index];
        GroupMemoryBarrierWithGroupSync();
    }

#if THREAD_WARP_SIZE < 16
    if (WaveGetLaneCount() < 16)
    {
        RankKeysWarpLT16(thread_index, GetSerializeIterations(), keys, offsets);

        if (thread_index < ONESWEEP_DIGIT_HALF_RADIX)
        {
            uint hist_reduction = WaveHistInclusiveScanCircularShiftWarpLT16(thread_index);
            gs_data[thread_index] = hist_reduction + (hist_reduction << 16);
            DeviceBroadcastReductionWarpLT16(thread_index, partition_index, hist_reduction);
        }

        WaveHistReductionExclusiveScanWarpLT16(thread_index);
        GroupMemoryBarrierWithGroupSync();

        UpdateOffsetsWarpLT16(thread_index, GetSerializeIterations(), offsets, keys);
        if (thread_index < ONESWEEP_DIGIT_RADIX)
            exclusive_hist_reduction = gs_data[thread_index >> 1] >> ((thread_index & 0x1) ? 16 : 0) & 0xffff;
        GroupMemoryBarrierWithGroupSync();
    }
#endif

    BlockScatterKeys(offsets, keys);
    LookBack(thread_index, partition_index, exclusive_hist_reduction);
    GroupMemoryBarrierWithGroupSync();

    if (partition_index < thread_blocks - 1)
        DeviceScatter(thread_index, partition_index, offsets);

    if (partition_index == thread_blocks - 1)
        DeviceScatterTail(thread_index, partition_index, offsets);
}
