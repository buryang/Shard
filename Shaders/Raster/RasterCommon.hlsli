#ifndef _RASTER_COMMON_INC_
#define _RASTER_COMMON_INC_

#include "../Math.hlsli"
#include "../GPUScene/GPUSceneCommon.hlsli"

/*view info for different view*/
struct ViewInfo
{
    float3x3 local_to_translated_world;
    float3x3 translated_world_to_local;

    //prev frame view matrix
    float3x3 prev_local_to_translated_world;
    uint flags;
};

//view related intermediate matrix
//store in LDS for fast access
struct ViewIntermediateData
{
    float4x4 local_to_clip;
    float4x4 translated_world_to_clip;
    float4x4 clip_to_translated_world;
    float3x3 local_to_translated_world;
    float3x3 translated_world_to_local;
    
    float4x4 prev_local_to_translated_world;
};

StructuredBuffer<ViewInfo> views;

struct ViewRange
{
    uint view_offset; //first view in views
    uint num_views;
};

struct Transform
{
    float3 translated_world_to_local;
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
    
    //half edge constants
    float2 edge01;
    float2 edge12;
    float2 edge20;
    
    float C0; //Calculate the corresponding value of each edge with
    float C1; //a unit step length of pixel, so precalc three cross value
    float C2; //then just step one for Y-axis and X-axis, reduce multiply times
    float3 depth_plane; //vert0 depth, vert1 delta_depth, vert2 delta_depth
    float3 inv_w;
    
    float3 barycentrics_dx;
    float3 barycentrics_dy;
    
    uint flags;
};

template<uint sub_pixel_samples, bool is_back_culling>
RasterTriangle SetupRasterTri(int4 scissor_rect, float4 verts[3])
{
    RasterTriangle tri;
    
    tri.flags |= RASTER_TRIANGLE_VALID_FLAG;
    tri.inv_w = rcp(float3(verts[0].w, verts[1].w, verts[2].w)); //
    
    float2 vert0 = verts[0].xy;
    float2 vert1 = verts[1].xy;
    float2 vert2 = verts[2].xy;
    
    tri.edge01 = vert0 - vert1;
    tri.edge12 = vert1 - vert2;
    tri.edge20 = vert2 - vert0;
    
    float delta_xy = tri.edge01.y * tri.edge20.x - tri.edge01.x * tri.edge20.y;
    tri.flags |= select(delta_xy >= 0.f, RASTER_TRIANGLE_BACK_FACE, 0);
    
    BRANCH
    if (tri.flags & RASTER_TRIANGLE_BACK_FACE)
    {
        if(is_back_culling)
        {
            tri.flags &= ~RASTER_TRIANGLE_VALID_FLAG;
        }
        else
        {
            tri.edge01 *= -1.0f;
            tri.edge12 *= -1.0f;
            tri.edge20 *= -1.0f;
        }
    }
    
    const float2 min_sub_pixel = min3(vert0, vert1, vert2);
    const float2 max_sub_pixel = max3(vert0, vert1, vert2);
    
    //round to pixel
    tri.min_pixel = (int2) floor((min_sub_pixel + sub_pixel_samples / 2 - 1) * (1.0f / sub_pixel_samples));
    tri.max_pixel = (int2) floor((max_sub_pixel - sub_pixel_samples / 2 - 1) * (1.0f / sub_pixel_samples));
    
    tri.min_pixel = max2(tri.min_pixel, scissor_rect.xy);
    tri.max_pixel = min2(tri.max_pixel, scissor_rect.zw - 1);
    
    
    if(any(tri.min_pixel > tri.max_pixel))
    { 
        //set to invalid
        tri.flags &= ~RASTER_TRIANGLE_VALID_FLAG;
    }
        
    //rebase to top-left
    const float2 base_sub_pixel = (float2) tri.min_pixel * sub_pixel_samples + (sub_pixel_samples / 2);
    vert0 -= base_sub_pixel;
    vert1 -= base_sub_pixel;
    vert2 -= base_sub_pixel;
    
    tri.C0 = tri.edge12.y * vert1.x - tri.edge12.x * vert1.y;
    tri.C1 = tri.edge20.y * vert2.x - tri.edge20.x * vert2.y;
    tri.C2 = tri.edge01.y * vert0.x - tri.edge01.x * vert0.y;
    
    //sum C before nudging for fill convention. afterwards it could be zero.
    const float scale_to_unit = sub_pixel_samples / (tri.C0 + tri.C1 + tri.C2);
    
    //precalc value for top-left rectangle pixel
    tri.C0 -= saturate(tri.edge12.y + saturate(1.f - tri.edge12.x));
    tri.C1 -= saturate(tri.edge20.y + saturate(1.f - tri.edge20.x));
    tri.C2 -= saturate(tri.edge01.y + saturate(1.f - tri.edge01.x));
    
    //scale C0/C1/C2 to use pixel incrments
    //testing pixel in triangle need only CX sign
    tri.C0 *= (1.0f / sub_pixel_samples);
    tri.C1 *= (1.0f / sub_pixel_samples);
    tri.C2 *= (1.0f / sub_pixel_samples);
    
    tri.barycentrics_dx = float3(-tri.edge12.y, -tri.edge20.y, -tri.edge01.y) * scale_to_unit;
    tri.barycentrics_dy = float3(tri.edge12.y, tri.edge20.y, tri.edge01.x) * scale_to_unit;
    
    tri.depth_plane.x = verts[0].z;
    tri.depth_plane.yz = float2(verts[1].z - verts[0].z, verts[2].z - verts[0].z) * scale_to_unit; //why
    
    return tri;
}

InstanceDynamicData CalcInstanceDynamicData(ViewInfo view, Instance instance)
{
    InstanceDynamicData dynamic_data;
    
    return dynamic_data;
}

//realize raster triangle according to follow url
//https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-rasterizer-stage-rules

template<typename WritePixelFunc>
inline void RasterTriangleScanline(RasterTriangle tri, WritePixelFunc write_func)
{
    float CY0 = tri.C0;
    float CY1 = tri.C1;
    float CY2 = tri.C2;
    
    float3 edge012 = { tri.edge12.y, tri.edge20.y, tri.edge01.y };
    bool3 is_open_edge = edge012 < 0u;
    float3 inv_edge012 = select(edge012 == 0u, 1.f / M_EPSILON, rcp(edge012));
    
    //traverse from miny to maxy
    for (int y = tri.min_pixel.y; y < tri.max_pixel.y; ++y)
    {
        //calc edge cross line y
        //float CX = CY - edgexx.y * (x - min_pixel.x);
        //=> edgexx.y * (x - min_pixel.x) <= CY
        //=> calc three cross point for three edge, then chose indexe n, n edge
        //=>  CYn / edgenx.y + min_pixel.x <= x <= CYm /edgemx.y + min_pixel.x
        float3 crossx = float3(CY0, CY1, CY2) * inv_edge012;
        
        float3 minx = select(is_open_edge, crossx, 0.0);
        float3 maxx = select(is_open_edge, tri.max_pixel.x - tri.min_pixel.x, crossx);
        
        //traverse from cross minx to cross maxx
        float xl = ceil(max3(minx.x, minx.y, minx.z)); //choose the minest crossx
        float xr = min3(maxx.x, maxx.y, maxx.z);
        
        float CX0 = CY0 - xl * tri.edge12.y;
        float CX1 = CY1 - xl * tri.edge20.y;
        float CX2 = CY2 - xl * tri.edge01.x;
        
        xl += tri.min_pixel.x;
        xr += tri.min_pixel.x;
        
        for (float x = xl; x < xr; ++x) //be care of VSN near page edges
        {
            if (min3(CX0, CX1, CX2) >= 0)
            {
                write_func(uint2(x, y), float3(CX0, CX1, CX2), tri);
            }
            
            CX0 -= tri.edge12.y;
            CX1 -= tri.edge20.y;
            CX2 -= tri.edge01.y;
        }
        
        CY0 += tri.edge12.x;
        CY1 += tri.edge20.x;
        CY2 += tri.edge01.x;
        
    }
}

template<typename WritePixelFunc>
inline void RasterTriangleRect(RasterTriangle tri, WritePixelFunc write_func)
{
    float CY0 = tri.C0;
    float CY1 = tri.C1;
    float CY2 = tri.C2;
    
    for(float y = tri.min_pixel.y; y < tri.max_pixel.y; ++y)
    {
        float CX0 = CY0;
        float CX1 = CY1;
        float CX2 = CY2;
        UNROLL
        for(float x = tri.min_pixel.x; x < tri.max_pixel.x; ++x)
        {
            if(min3(CX0, CX1, CX2) >= 0)
            {
                write_func(uint2(x, y), float3(CX0, CX1, CX2), tri);
            }
            
            CX0 -= tri.edge12.y;
            CX1 -= tri.edge20.y;
            CX2 -= tri.edge01.y;
        }
        
        CY0 += tri.edge12.x;
        CY1 += tri.edge20.x;
        CY2 += tri.edge01.x;
    }
}

//handle clipping triangle impicitly by paper
//to do: https://groups.csail.mit.edu/graphics/classes/6.837/F03/lectures/15%20Raster.pdf 
inline bool IsInsideTriangle(float4 vc0, float4 vc1, float4 vc2, float2 pixel)
{
    float3 pixel_homo = float3(pixel + 0.5f, 1.0f);
    float e01 = determinant(float3x3(vc0.xyw, vc1.xyw, pixel_homo));
    float e12 = determinant(float3x3(vc1.xyw, vc2.xyw, pixel_homo));
    float e20 = determinant(float3x3(vc2.xyw, vc0.xyw, pixel_homo));
    return all(float3(e01, e12, e20) >= 0);
}

template<typename WritePixelFunc>
inline void RasterTriangleHomogeneous(float4 verts[3], WritePixelFunc write_func)
{
}    


//using rect raster for small triangles
#define RASTER_SCANLINE_MIN_PXIELS 4 

template<typename WritePixelFunc>
void RasterTriangle(RasterTriangle tri, WritePixelFunc write_func)
{
    const bool use_scanline = WaveActiveAnyTrue(tri.max_pixel.x - tri.min_pixel.x > RASTER_SCANLINE_MIN_PXIELS); //using same algorithm for one wave
    if(use_scanline)
        RasterTriangleScanline(tri, write_func);
    else
        RasterTriangleRect(tri, write_func);
}


////to do conservative raster
//https://developer.nvidia.com/gpugems/gpugems2/part-v-image-oriented-computing/chapter-42-conservative-rasterization 

#endif