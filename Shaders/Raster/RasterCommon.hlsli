#ifndef _RASTER_COMMON_INC_
#define _RASTER_COMMON_INC_

#include "../GPUScene/GPUSceneCommon.hlsli"

/*view info for different view*/
struct ViewInfo
{
    float3x3 local_to_translated_world;
    float3x3 translated_world_to_local;
    uint flags;
};

StructuredBuffer<ViewInfo> views;

struct Transform
{
    float3 world_to_local;
    //todo
};

/*transformed vertex data set*/
struct TransformedVertex
{
    float3 position_local;
    float3 position_post_deform;
    float3 prev_position_post_deform;
    float3 position_world;
    float3 position_clip;
};

#define VERTEX_CACHE_SIZE 256 //todo set to thread group size
//transformed vertex roll group cache
groupshared float3 cache_vertex_position_local[VERTEX_CACHE_SIZE];
groupshared float3 cache_vertex_position_post_deform[VERTEX_CACHE_SIZE];
groupshared float3 cache_vertex_prev_position_post_deform[VERTEX_CACHE_SIZE];
groupshared float3 cache_vertex_position_world[VERTEX_CACHE_SIZE];

inline uint GetCacheVertexIndex(uint thread_index)
{
    return thread_index & 0x3fu; 
}

TransformedVertex LoadTransformedVertexLDS(uint thread_index)
{
    TransformedVertex trans_vertex = (TransformedVertex) 0;
    //todo
    return trans_vertex;
}

void StoreTransformedVertexLDS(in TransformedVertex vertex, uint thread_index)
{
    const uint cache_index = GetCacheVertexIndex(thread_index);
    cache_vertex_position_local[cache_index] = vertex.position_local;
    //todo

}

struct CachedVertex
{
    TransformedVertex transformed_vertexs;
    float point_subpixel_clip;//todo
};


struct RasterInfo
{
    uint4 scissor;
    uint lod_level;
    uint packed_flags;
    
    //render target virtual and physical address
};

struct RasterBinData
{
        
};

struct RasterBinMeta
{
    uint cluster_offset;
    //todo
};

/*
* gpu global raster uniform buffer struct
*/
struct GPURasterUBO
{
    uint raster_bin_data_bf_index; //compacted bin data buffer
    uint raster_bin_meta_bf_index; //meta information for each bin
    uint visible_cluster_bf_index; //visible cluster buffer
};

//calculate Instance related transformation


#define RASTER_TRIANGLE_VALID_FLAG  1u << 1
#define RASTER_TRIANGLE_BACK_FACE   1u

struct RasterTriangle
{
    uint2 min_pixel;
    uint2 max_pixel;
    
    float2 edge01;
    float2 edge12;
    float2 edge20;
    
    float3 color;
    float3 depth_plane; //vert0 depth, vert1 delta_depth, vert2 delta_depth
    float3 inv_w;
    
    float3 barycentrics_dx;
    float3 barycentrics_dy;
    
    uint flags;
};

//todo 
RasterTriangle SetupRasterTri()
{
}

InstanceDynamicData CalcInstanceDynamicData(ViewInfo view, Instance instance)
{
    
}


#endif