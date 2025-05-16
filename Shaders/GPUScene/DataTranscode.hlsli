#ifndef _DATA_TRANSCODE_INC_
#define _DATA_TRANSCODE_INC_
/**
* realize vertex/index compression like Dense Geometry Format(DGF) etc.
* unreal engine nanite has two representation(memory/disk), memory represent main
* goal for decoding-instant and random accessing, while disk representation main 
* goal for triangle strip compresssion. I think DGF is more like disk representation,
* but its main advantage is for memory representation for ray tracing
*/

//whether enable the DGF compression 
#define CLUSTER_DGF_COMPRESS_ENABLE
#define CLUSTER_VERTEX_COMPRESS_ENABLED 1


struct TranformedLocalVertex
{
    //todo    
};

//triangle index remap
struct TriRemapEntry
{
    uint input_triangle_index;
    
    uint index0;
    uint index1;
    uint index2; //
};

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
    
};


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

#endif

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