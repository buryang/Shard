#include "../CommonUtils.hlsli"
#include "../Math.hlsli"
#include "../ComputeShaderUtils.hlsli"
#include "../GPUScene/GPUSceneCommon.hlsli"
#ifdef SCENE_STREAMING_ENABLE
#include "../GPUScene/StreamingCommon.hlsli"
#endif

struct ClusterInfo
{
    uint instanceID;
    uint clusterID;//
};

groupshared uint group_base_vertex_index;
groupshared uint2 group_used_vertex_mask;

void DeduplicateVertIndex(uint3 vert_indexes, uint group_thread_index, bool is_tri_valid, out uint unique_verts_num, out uint lane_vert_index, out uint3 corner_lane_indexes)
{
    const uint lane_count = WaveGetLaneCount();
    const bool multi_wave_needed = lane_count < 32;
    uint base_vertex_index;
    
    BRANCH
    if (multi_wave_needed)
    {
        if(group_thread_index == 0u)
        {
            group_base_vertex_index = 0xffffffffu;
            group_used_vertex_mask = 0u;
        }
        GroupMemoryBarrierWithGroupSync();
        if (is_tri_valid)
        {
            uint wave_base_vertex_index = WaveActiveMin(vert_indexes.x); 
            if (WaveIsFirstLane())
            {
                InterlockedMin(group_base_vertex_index, wave_base_vertex_index);
            }
        }
        GroupMemoryBarrierWithGroupSync();
        base_vertex_index = group_base_vertex_index;
    }
    else
    {
        base_vertex_index = WaveActiveMin(is_tri_valid ? vert_indexes.x : 0xffffffffu); //always x is smallest
    }
    
    uint2 local_used_vertex_mask = 0u;
    if(is_tri_valid)
    {
        vert_indexes -= base_vertex_index;
        UNROLL
        for (uint i = 0u; i < 3; ++i)
        {
            bool is_lower = vert_indexes[i] < 32;
            uint dst_mask = 1u << (vert_indexes[i] & 31);
            if(is_lower)
                local_used_vertex_mask.x |= dst_mask;
            else
                local_used_vertex_mask.y |= dst_mask;
        }

    }
    
    uint2 used_vertex_mask = 0u;
    BRANCH
    if (multi_wave_needed)
    {
        uint2 temp_mask = WaveActiveBitOr(used_vertex_mask);
        if (WaveIsFirstLane())
        {
            InterlockedOr(group_used_vertex_mask.x, temp_mask.x);
            InterlockedOr(group_used_vertex_mask.y, temp_mask.y);
        }
        GroupMemoryBarrierWithGroupSync();
        used_vertex_mask = group_used_vertex_mask;
    }
    else
    {
        used_vertex_mask.x = WaveActiveBitOr(local_used_vertex_mask.x);
        used_vertex_mask.y = WaveActiveBitOr(local_used_vertex_mask.y);
    }
    corner_lane_indexes.x = xx;
    corner_lane_indexes.y = xx;
    corner_lane_indexes.z = xx;
    lane_vert_index = base_vertex_index + FindNthSetBit(used_vertex_mask, group_thread_index);
    unique_verts_num = countbits(used_vertex_mask);
}

//main entry for raster
void ClusterRaster(uint cluster_index, uint group_thread_index : SV_GroupIndex)
{
    
}

void main()
{
    
}