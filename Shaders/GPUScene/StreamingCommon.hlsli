#ifndef _STREAM_COMMON_INC_
#define _STREAM_COMMON_INC_

#include "ClusterCommon.hlsli"

// we tag the 64 bit VA of a cluster group with a special address range
// when it hasn't been loaded yet, values greater than this encode the
// frame index that this group has been requested last.
#define STREAMING_INVALID_ADDRESS_START (uint64_t(1) << 63)

// Must be 32 to se we have easier processing of the bit arrays.
// the minimum allocation will use 32 bits, which can only straddle across two u32 values
#define STREAMING_ALLOCATOR_MIN_SIZE 32

//must be power of 2
#define STREAM_UPDATE_SCENE_WORKGROUP 64
#define STREAM_AGEFILTER_GROUPS_WORKGROUP 128
#define STREAM_COMPACTION_NEW_CLAS_WORKGROUP 128
#define STREAM_COMPACTION_OLD_CLAS_WORKGROUP 64
#define STREAM_ALLOCATOR_LOAD_GROUPS_WORKGROUP 64
#define STREAM_ALLOCATOR_UNLOAD_GROUPS_WORKGROUP 64
#define STREAM_ALLOCATOR_BUILD_FREEGAPS_WORKGROUP 64
#define STREAM_ALLOCATOR_FREEGAPS_INSERT_WORKGROUP 64
#define STREAM_ALLOCATOR_SETUP_INSERTION_WORKGROUP 64

//update flags bitmask define 
#define STREAMING_UPDATE_OFFSET_MASK    0xfffffffcu
#define STREAMING_UPDATE_LEAF_MASK      0x00000001u
#define STREAMING_UPDATE_RESET_MASK     0x00000002u

#define STREAMING_STATS_READBACK 1

struct AllocatorRange
{
    int count;
    uint offset;
};

struct AllocatorStats
{
    uint64 allocated_size;
    uint64 wasted_size;
};

struct StreamingAllocator
{
    uint free_gap_count;
    uint granularity_byte_shift; //size of one unit in (1<<shift) byte
    uint max_allocation_unit_size; //max allocation size in unit
    uint sector_count;
    uint sector_max_allocation_sized;
    uint sector_size_shift;
    uint base_wasted_size;
    uint used_bits_count;
    //todo
    BUFFER_REF(uint_inout) free_gap_positions;
    BUFFER_REF(uint_inout) free_gap_size;
    BUFFER_REF(uint_inout) free_gap_position_binned;
    BUFFER_REF(AllocatorRange_input) free_size_ranges;
    BUFFER_REF(uint_inout) used_bits;
    BUFFER_REF(uint_inout) used_sector_bits;
    BUFFER_REF(AllocatorStats_inout) stats;
};

struct CLASBuildInfo
{
    uint clusterID;
    uint clusterFlags;
    uint packed;
    uint base_primitive_index_and_flags;
    uint index_buffer_stride;
    uint vertex_buffer_stride;
    uint primitive_index_and_flags_buffer_stride;
    uint opacity_miacromap_index_buffer_stride;
    uint64 vertex_buffer;
    uint64 index_buffer;
    uint opacity_micromap_array;
    uint opacity_micromap_index_buffer;
};

struct StreamingUpdate
{
    //unload operations are before load operations
    uint patch_unload_group_count;
    //total operations
    uint patch_group_count;
    
    //all newly loaded groups have linear positions in the
    //compacted list of active groups starting with this value
    uint load_active_group_offset;
    uint load_active_cluster_offset;
    
    uint task_index;
    uint64 frame_index;
    
    //compaction
    uint move_clas_count;
    uint64 move_clas_size;
    
    //buffer 
#if USE_GPUSCENE_RAYTRACING
    
#endif
};

//todo stream request priority included in group info
struct StreamingRequest
{
    uint max_loads;
    uint max_unloads;
    uint load_count;
    uint unload_count;
    uint64 frame_index;
    BUFFER_REF(half2_inout) load_groups;
    BUFFER_REF(half2_inout) unload_groups;
};

struct StreamingPatch
{
    uint primitiveID;
    uint group_index;
    uint64 group_address; //
};

struct StreamingGroup
{
    uint cluster_count;
    int age;
    BUFFER_REF(Group_in) group;
    
    void Init(uint cluster_count_in, uint64 group_address)
    {
        cluster_count = cluster_count_in;
        age = 0u;
        group = group_address;
    }
};

struct StreamingResident
{
    //Dynamic content:
    //
    //These are lists of groups that don't include
    //lowest detail, as those are always kept resident
    uint active_group_count;
    uint active_cluster_count;
    
    BUFFER_REF(StreamingGroup_in) active_groups;
    
    //Resident content:
    //
    //Due to immutability of the residentID, these tables
    //are indexed sparsely, and they do contain all
    //content, including persistent lowest detail.
};

#ifdef STREAMING_STATS_READBACK

struct StreamingStatsReadback
{
    uint64 max_data_bytes;
    uint64 reserved_data_bytes;
    uint64 used_data_bytes;
    
    uint64 reserved_cluster_bytes;
    uint64 used_clas_bytes;
    uint64 wasted_clas_bytes;
    uint max_sized_left;
    uint max_size_reserved;
    
    uint64 max_transfer_bytes;
    uint64 transfer_bytes;
    uint transfer_count;
    uint load_count;
    uint unload_count;
    uint uncompleted_load_count;
    uint max_load_count;
    uint max_unload_count;
};

#endif

struct SceneStreaming
{
    int age_threshold;
    uint frame_index;
    StreamingResident resident;
    StreamingUpdate update;
    StreamingRequest request;
    StreamingAllocator allocator;
    
    void Init()
    {
        
    }
};

#endif //_STREAM_COMMON_INC_