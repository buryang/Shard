#ifndef _GPUSCENE_INC_
#define _GPUSCENE_INC_

#include "../CommonUtils.hlsli"
#include "../BindlessResource.hlsli"

#define USE_GPUSCENE_STREAMING 1
#define USE_GPUSCENE_RAYTRACING 1

#if USE_GPUSCENE_STREAMING
#include "StreamingCommon.hlsli"
#endif
#include "ClusterCommon.hlsli"
#include "ViewCommon.hlsli"
#include "DataTranscode.hlsli"

#define MAX_CUSTOM_PRIMITIVE_DATA_COUNT 16
#define MAX_SCENE_INSTANCE_COUNT (1u << 24)
#define MAX_PRIMITIVE_NODE_COUNT  (1u << 16)

struct AABB
{
    float3 min;
    float3 max;
};

struct BoundingBoxSphere
{
    AABB bound_box;
    float4 sphere;
};

struct Instance
{
    uint primitiveID;
    float3x3 local_to_world;
    float3x3 world_to_local;
    float3x3 prev_local_to_world;
    float3x3 prev_world_to_local;
    AABB bounding_box;
    uint flags;
    uint4 custom_payload;
};

struct Primitive
{
    uint flags;
    uint primitiveID;
    uint instance_payload_stride; //instance data size for primitive
    uint persistent_primitive_index;
    
    uint clusters_count;
    uint groups_count;
    uint nodes_count;
    uint padding; //data padding
    
    BBox bbox; //object space primitive bounding box
    
    //lod hierarchy traversal, geometry DAG node
    BUFFER_REF(Nodes_in) nodes;
    
    //streaming component
    //provides memory address of a resident group.
    //
    //Note this 64-bit value uses a special encoding.
    //only addresses < STREAMING_INVALID_ADDRESS_BEGIN can be dereferenced.
    BUFFER_REF(uint64s_inout) streaming_group_addresses;
    //material IDs, unreal engine one primitive has 64 materials at most
    
    //todo
    float4 custom_data[MAX_CUSTOM_PRIMITIVE_DATA_NUM];
};

#define INSTANCE_ELEMENT_DATA_SIZE sizeof(Instance)
#define PRIMITIVE_ELEMENT_DATA_SIZE sizeof(Primitive)
#define INVALID_INSTANCE_DATA_OFFSET 0xffffffffu

//GPU Scene
struct GPUScene
{
    BUFFER_REF(uint_in) instances;
    BUFFER_REF(uint_in) instance_payloads;
    BUFFER_REF(uint_in) primitives;
    BUFFER_REF(uint_in) lightmap;
    BUFFER_REF(uint_in) lights;
    
    
    uint instance_soa_stride;
    uint num_instances;
    uint num_primitives;

#if USE_GPUSCENE_STREAMING
    SceneStreaming streaming; //todo seperate this or included here
#endif
    
    SceneViews views;
    
    void Init()
    {
#if USE_GPUSCENE_STREAMING
        streaming.Init();
#endif
    }
};

//instance SOA payload offsets in float4s
struct InstancePayloadOffsets
{
    uint hierarchy_offset;
    uint editor_data;
    uint local_bounds;
    uint dynamic_data;
    uint skinning_data;
    uint light_shadow_uv_bias;
    uint payload_extension;
    uint custom_data;
    
    void Init()
    {
        hierarchy_offset = INVALID_INSTANCE_DATA_OFFSET;
        editor_data = INVALID_INSTANCE_DATA_OFFSET;
        local_bounds = INVALID_INSTANCE_DATA_OFFSET;
        dynamic_data = INVALID_INSTANCE_DATA_OFFSET;
        skinning_data = INVALID_INSTANCE_DATA_OFFSET;
        light_shadow_uv_bias = INVALID_INSTANCE_DATA_OFFSET;
        payload_extension = INVALID_INSTANCE_DATA_OFFSET;
        custom_data = INVALID_INSTANCE_DATA_OFFSET;
    }
};

struct GPUBindlessInfo
{
    uint persistent_memory_bf_index;
    uint persistent_memory_used_bits_bf_index;
    uint traversal_info_bf_index;
    uint traversal_info_size;
    uint render_cluster_info_bf_index; //cluster 
    uint render_cluster_info_size;
    uint extent_payload_bf_index0;
    uint extent_payload_bf_index1;
    
    void Init()
    {
        persistent_memory_bf_index = INVALID_INSTANCE_DATA_OFFSET;
        persistent_memory_used_bits_bf_index = INVALID_INSTANCE_DATA_OFFSET;
        traversal_info_bf_index = INVALID_INSTANCE_DATA_OFFSET;
        traversal_info_size = INVALID_INSTANCE_DATA_OFFSET;
        render_cluster_info_bf_index = INVALID_INSTANCE_DATA_OFFSET; 
        render_cluster_info_size = INVALID_INSTANCE_DATA_OFFSET;
        extent_payload_bf_index0 = INVALID_INSTANCE_DATA_OFFSET;
        extent_payload_bf_index1 = INVALID_INSTANCE_DATA_OFFSET;
    }
};

//static const GPUScene gpu_scene = { }; //todo initialize this

#define GPU_SCENE_BUFFER_RW(index) GetBindlessRWByteBufferUniform(index)
#define GPU_SCENE_VERTEX_INFEX(index) (index.extent_payload_index) //todo

inline RWByteAddressBuffer GetGPUScenePersistentMemory(GPUBindlessInfo index)
{
    return GPU_SCENE_BUFFER_RW(index.persistent_memory_bf_index);
}

inline RWByteAddressBuffer GetGPUScenePersistentMemoryUsedBits(GPUBindlessInfo index)
{
    return GPU_SCENE_BUFFER_RW(index.persistent_memory_used_bits_bf_index);
}

Primitive LoadPrimitive(uint primitiveID)
{
    Primitive primitive = (Primitive) 0;
    
    //todo
    return primitive;
}

//how to save instance?? SOA or ??
Instance LoadInstance(uint instanceID)
{
    Instance instance = (Instance) 0;
    instance.primitiveID = asuint(LoadPrimitiveSceneRawData());
    
    return instance;
}

InstancePayloadOffsets GetInstancePayloadOffsets(uint )
{
    InstancePayloadOffsets offsets = (InstancePayloadOffsets) 0;
    //todo
    return offsets;
}


#endif //_GPUSCENE_INC_

