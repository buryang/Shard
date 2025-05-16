#ifndef _RASTER_COMMON_INC_
#define _RASTER_COMMON_INC_

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
    
};

struct TransformedVertex
{
    
};

#define VERTEX_CACHE_SIZE 120 //(max_tess_factor+1)*(max_tess_factor+2)/2
groupshared float3 group_cache_vertices[VERTEX_CACHE_SIZE];

struct CachedVertex
{
    TransformedVertex transformed_vertexs;
    float point_subpixel_clip;//todo
};


#endif