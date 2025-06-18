#ifndef _CLUSTER_COMMON_INC_
#define _CLUSTER_COMMON_INC_
#include "../CommonUtils.hlsli"

#pragma target 6.2 //for 6.2 support int8 and int64
#pragma enable_dxc_dx12_compatibility

#define uint64 uint64_t
#define int64 int64_t
#define uint16 uint16_t
#define int16 int16_t 
//#define uint8 

#define CLUSTER_VERTEX_COUNT 32 
#define CLUSTER_TRIANGLE_COUNT 32

#define TESSELLATED_CLUSTER_VERTEX_COUNT 32
#define TESSELLATED_CLUSTER_TRIANGLE_COUNT 32
#define TESSELLATED_LOOKUP_SIZE 16
#define TESSELLATED_SIZE 11
#define TESSELLATED_MAX_TRIANGLES (TESSELLATED_SIZE * TESSELLATED_SIZE)
#define TESSELLATED_MAX_VERTICES ((TESSELLATED_SIZE+1)*(TESSELLATED_SIZE+2))/2)
#define TESSELLATED_COORD_MAX (1<<15)

#define MATERIAL_ID_BITS_SHIFT 14
#define MATERIAL_ID_MAX_NUM (1 << MATERIAL_ID_BITS_SHIFT)
#define MATERIAL_SLOW_PATH_FLAG (1u << 31) //todo


#define NODE_MAX_CHILD_COUNT 4
#define GROUP_MAX_CLUSTER_COUNT 8


#define INVALID_DATA_OFFSET 0xffffffffu


struct BBoxSphere 
{
    float3 low;
    float3 high;
    float4 sphere; //xyz-center w-radius
};

struct Cluster
{
    uint triangle_count_minus1;
    uint vertex_count_minus1;
    uint lod_level;
    uint group_child_index;
    uint groupID;
    uint packed_flags; //todo
    
    BBoxSphere bounding_box;
    
    float lod_error;
    float edge_length;
    
    
    //todo delete packed cluster
    //encoded attributes should not be float3
    //BUFFER_REF(float3_in) positions;
    //BUFFER_REF(float3_in) normals;
    //BUFFER_REF(uint8_in) local_triangles;
    
    //material table, slow path/3 materials fast path
    
};

//like unreal engine visibleClustersssssss
struct VisibleCluster
{
    uint packed_flags;
    uint viewID; //view ID for multi view taversal
    uint groupID; //group the cluster in
    uint instanceID; //object instance ID
    uint clusterID; //todo clusterID array to save cluster address?
};

//A group contains multiple clusters that are the result of
//a common mesh decimation operation. Clusters within a group
//are watertight to each other. Groups are always streamed in
//completely, which simplifies the streaming management.
//Real-Time Ray Tracing of Micro-Poly Geometry with Hierarchical Level of Detail

struct TraversalMetric
{
    //scalar by design, avoid hiccups with packing
    //order must match `nvclusterlod::Node`
    float bounding_sphereX;
    float bounding_sphereY;
    float bounding_sphereZ;
    float bounding_sphere_radius;
    float max_quadric_error;
};

#define NODE_PACKED_IS_GROUP_SHIFT 0
#define NODE_PACKED_IS_GROUP_BITS 1

#define NODE_PACKED_CHILD_OFFSET 1
#define NODE_PACKED_CHILD_BITS 26
#define NODE_PACKED_CHILD_COUNT_MINUS_ONE_OFFSET 27
#define NODE_PACKED_CHILD_COUNT_MINUS_ONE_BITS 5

#define NODE_PACKED_GROUP_INDEX_OFFSET 1
#define NODE_PACKED_GROUP_INDEX_BITS 23
#define NODE_PACKED_GROUP_INDEX_CLUSTER_COUNT_MINUS_ONE_OFFSET 24
#define NODE_PACKED_GROUP_INDEX_CLUSTER_COUNT_MINUS_ONE_BITS 8

#define MAX_NODE_HIERACHY_CHILDREN_COUNT (1u << 6)

//one node map to one cluster
struct Node
{
    BBoxSphere bounding_box;
    uint child_group_reference; //child node/cluster reference
    uint is_leaf_loaded; //is_leaf/loaded
};

//DAG node
struct Group
{
    uint geometryID;
    uint groupID;
    
    //streaming: global unique id given on load
    //clusters array starts directly after group
    //preloaded: local id within geometry
    uint residentID;
    uint cluster_residentID;
    
    //when this group is first loaded, this is where
    //the temporary clas builds start
    uint streaming_new_build_offset;
    
    uint lod_level;
    uint cluster_count;
    
    BBoxSphere bounding_box;
    
    TraversalMetric traversal_metric;
    
    BUFFER_REF(uint32s_in) cluster_generating_groups;
    BUFFER_REF(BBoxes_In) cluster_bboxes;
};


#if USE_GPUSCENE_RAYTRACING
//raytrace AS build infos
struct BlasBuildInfo
{
    uint cluster_ref_count; //the number of CLAS this blas reference
    uint cluster_ref_stride; //stride of array(typically 8 for 64bit)
    uint cluster_reference; //start address of the array
};

struct TLASInfo
{
    float4x4 transform;
    
};
#endif


//A subdivision configuration (for example edge subdivisions: 1x2x3)
//refers to a range of triangles and vertices within
//the TessellationTable
struct TessTableEntry
{
    uint first_triangle;
    uint first_vertex;
    uint triangle_count;
    uint vertices_count;
};

/** alogorithm "Optimized Pattern-Based Adaptive Mesh Refinement Using GPU"*/
struct TessellationTable
{
    BUFFER_REF(uint_in) vertices;  //u,v barycentrics packed as 2 x 16 bit, 1 << 15 represents 1.0
    BUFFER_REF(uint_in) triangles; //3 x 8 bit indices packed in 32 bit
    //these arrays are indexed by a power of 2 lookup table logic, so that we can quickly
    //find patterns for a triangle that is subdivided e.g. 11 x 7 x 10
    BUFFER_REF(TessTableEntry_in) entities;
    
    /*********************************************************/
    
    //clas template related data
    BUFFER_REF(uint64_in) template_address; //cluster template address for that pattern
    //as well as their worst-case instantiation size, so we can estimate the size of the resulting CLAS
    BUFFER_REF(uint_in) template_instantiation_sizes; //max cluster instantiation size for the pattern
    
};

/*readback info*/
struct CLASBuildStatReadBack
{
    uint traversal_info_count;
    uint rendered_group_count;
    uint rendered_cluster_count;
    uint rendered_triangle_count;
    
    uint64 blas_size; 
   
};

struct TessellatedStatReadBack
{
    uint visible_cluster_count;
    uint full_cluster_count;
    
    uint split_triangle_count;
    uint part_triangle_count;
    
    uint total_triangle_count;
    uint temp_instantiation_count;
    
    uint generated_vertex_count;
    uint blas_cluster_count;
    
    uint trans_build_count;
    uint trans_part_triangle_count;
    
    uint actual_trans_build_count;
    uint actual_temp_instantiation_count;
    
    uint64 gen_data_count;
    uint64 gen_actual_data_count;
    
    uint blas_blas_reserved_size;
    uint blas_actual_size;
    
    uint cluster_triangleID;
    uint _cluster_triangleID_packed;
    uint instanceID;
    uint _instanceID_packed;
};

//triangle index remap
struct TriRemapEntry
{
    uint input_triangle_index;
    
    uint index0;
    uint index1;
    uint index2; //
};

#endif