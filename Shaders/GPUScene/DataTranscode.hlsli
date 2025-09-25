#ifndef _DATA_TRANSCODE_INC_
#define _DATA_TRANSCODE_INC_

#include "ClusterCommon.hlsli"
/**
* realize vertex/index compression like Dense Geometry Format(DGF) etc.
* unreal engine nanite has two representation(memory/disk), memory represent main
* goal for decoding-instant and random accessing, while disk representation main 
* goal for triangle strip compresssion. I think DGF is more like disk representation,
* but its main advantage is for memory representation for ray tracing
*/

//BVH BLAS compaction
//https://developer.nvidia.com/blog/tips-acceleration-structure-compaction/

//whether enable the DGF compression 
#define CLUSTER_DGF_COMPRESS_ENABLE
#define CLUSTER_VERTEX_COMPRESS_ENABLED 1
#define CLUSTER_TRIANGLE_CONNECTIVITY_ENABLED 1

//node bit mask definiation
#define NODE_CHILD_NODE_OFFSET 1
#define NODE_CHILD_NODE_BITS 26
#define NODE_CHILD_NODE_COUNT_MINUS1_OFFSET 26
#define NODE_CHILD_NODE_COUNT_MINUS1_BITS 5
#define NODE_CHILD_GROUP_INDEX_OFFSET 1
#define NODE_CHILD_GROUP_INDEX_BITS 23
#define NODE_CHILD_GROUP_CLUSTERS_MINIUS1_OFFSET 23
#define NODE_CHILD_GROUP_CLUSTERS_MINIUS1_BITS 8


struct TranformedLocalVertex
{
    //todo    
};


#if 0
//encoded cluster and group bitstream info
struct PackedClusterInfo
{
    uint flags;
    uint byte_buffer_offset; //offset of the cluster
    uint num_vertexs; //vertices count
    uint num_indexes; //index count
    uint position_buffer_offset;
    uint index_buffer_offset;
    uint3 position_bits; //position encoded bits
    uint index_bits; //index offset encoded bits
    uint normal_bits; //normal bits
    
    BBox bounding_box;
};

struct PackedGroupInfo
{
    uint flags;
    uint num_clusters;
    //cluster info 
    
    BBox bounding_box;
    
    //LOD related error
    float min_LOD_error;
    float max_parent_LOD_error;
};

struct PackedViewInfo
{
    int4 view_rect;//view rectangle
    uint streaming_priority_category_flags;
    int lighting_channel_mask;
};

#endif

#ifdef CLUSTER_DGF_COMPRESS_ENABLE

//triangle topology encoding
#define TRIANGLE_TOPOLOGY_RESTART 0u
#define TRIANGLE_TOPOLOGY_EDGE1 1u
#define TRIANGLE_TOPOLOGY_EDGE2 2u
#define TRIANGLE_TOPOLOGY_BACKTRACK 3u

void UnpackDGFBlockForTriangle(ByteAddressBuffer encode_data, uint block_offset, uint primitive_index, out uint index[3], out float3 vertex[3])
{
    
}
#endif

void PackNode(uint node_child_offset, uint node_child_count, out Node node)
{
    uint packed = 1u << 31; //node flag
    PackBits(packed, node_child_offset, NODE_CHILD_NODE_OFFSET, NODE_CHILD_NODE_BITS);
    PackBits(packed, node_child_count - 1, NODE_CHILD_NODE_COUNT_MINUS1_OFFSET, NODE_CHILD_NODE_COUNT_MINUS1_BITS);
    node.child_group_reference = packed;
    node.is_leaf_loaded = 0u; //todo
}

void PackNode(uint group_index, uint group_cluster_count, out Node node)
{
    uint packed = 0u;
    PackBits(packed, group_index, NODE_CHILD_GROUP_INDEX_OFFSET, NODE_CHILD_GROUP_INDEX_BITS);
    PackBits(packed, group_cluster_count - 1, NODE_CHILD_NODE_COUNT_MINUS1_OFFSET, NODE_CHILD_NODE_COUNT_MINUS1_BITS);
    //todo
    node.child_group_reference = packed;
}

void UnpackNode(in uint2 packed, out Node node)
{
    
}

void PackGroup(Group group)
{
    
}

void UnpackGroup(in uint4 packed, out Group group)
{
    
}

void UnpackClusterInfo(in ClusterHeader cluster_header, out ClusterInfo cluster_info)
{
    //todo
}

void UnpackGroupInfo(out GroupInfo group_info)
{
    
}

#if CLUSTER_VERTEX_COMPRESS_ENABLED
uint2 FetchPackedVertexPosition(in PackedClusterInfo cluster, uint vertex_index)
{
    
}

void UnpackedCompressedPosition(uint2 vertex_compressed, in PackedClusterInfo cluster, out float3 vertice)
{
    
}
#endif //CLUSTER_VERTEX_COMPRESS_ENABLED

#if CLUSTER_TRIANGLE_CONNECTIVITY_ENABLED
//unreal engine encode triangle index [base_index, delta0, delta1]
uint3 UnpackCompressedTriangleVertexIndex(uint tri_index, in PackedClusterInfo cluster)
{
    
}
#else
#endif //CLUSTER_TRIANGLE_CONNECTIVITY_ENABLED

void UnpackVertexPosition(in PackedClusterInfo cluster, uint vertex_index, out float3 vertice)
{
#if CLUSTER_VERTEX_COMPRESS_ENABLED
    const uint2 packed_position = FetchPackedVertexPosition(cluster, vertex_index);
    UnpackedCompressedPosition(packed_position, cluster, vertice);
#else
#endif
}

void UnpackTriangle(in PackedClusterInfo cluster, uint3 triange_vertex_index, xx)
{

}

inline float4 LoadPrimitiveSceneRawData(uint primitive_index, uint item_index)
{
    uint target_index = primitive_index + item_index;
    return gpu_scene.primitives.Load4(target_index);
}

inline float4 LoadInstanceSceneRawData(uint instance_index, uint item_index)
{
    uint target_index = 0u;
    return gpu_scene.instances.Load4(target_index);
}


//quantize & dequantize functions
//WCP algorithm?

#endif //_DATA_TRANSCODE_INC_