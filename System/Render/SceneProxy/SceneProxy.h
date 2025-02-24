#pragma once
#include "Core/EngineGlobalParams.h"
#include "Utils/CommonUtils.h"
#include "HAL/HALCommandIR.h"
#include "Scene/Scene.h"
#include "../GPUUploadSystem.h"

namespace Shard::Renderer
{
    class GPUSceneStreaming;
    class GPUSceneProxy;
    class GPUDataUploader;

    //instance data size limits
    REGIST_PARAM_TYPE(UINT, RENDER_SYSTEM_INSTANCE_LIMITS, 1024 * 1024 * 1024);
    //primitive data size limits
    REGIST_PARAM_TYPE(UINT, RENDER_SYSTEM_PRIMITIVE_LIMITS, 1024 * 1024 * 1024);

    //config for tessellation, number of segments per edge <= 11
    REGIST_PARAM_TYPE(BOOL, RENDER_SYSTEM_TESSELLATION_ENABLED, true);
    REGIST_PARAM_TYPE(UINT, RENDER_SYSTEM_TESSELLATION_EDGE_SEGMENT, 11);
    //total number of vertices across all subdivision patterns
    REGIST_PARAM_TYPE(UINT, RENDER_SYSTEM_TESSELLATION_MAX_VERTICES, 256);
    REGIST_PARAM_TYPE(UINT, RENDER_SYSTEM_TESSELLATION_MAX_CONFIGS, 11 * 11);

    enum class EPrimitiveState
    {
        eAdded, //need allocate gpu slot
        eDirtyTransform,
        eDirtyOther, //need re-allocate gpu slot 
        eRemoved,
    };

    //record gpu scene info of primitive
    struct ECSPrimitiveGPUInfo
    {
        uint32_t    data_offset_{ 0u };
        uint32_t    num_instances_{ 1u };
        uint32_t    index_{ ~0u };

    };

    //primitive draw command
    struct PrimitiveBatchDrawCommand
    {
        //todo pso binding
        //todo vector
    };

    class SceneProxy
    {
    public:
        SceneProxy() = default;
        DISALLOW_COPY_AND_ASSIGN(SceneProxy);
        void Init(Scene::WorldScene* world);
        void UnInit();
        /*whether scene supported CLAS scene*/
        bool IsCLASSupported()const;
        size_type GetCLASGeometryCount()const;
        void Tick(float delta_time);
    private:
        GPUSceneProxy* gpu_scene_{ nullptr };
        //todo
        //Vector<> geometries_;
        //Vector<> clas_geometries_;
    };


    struct Cluster
    {
        uint32_t triangle_count_minus1;
        uint32_t vertex_count_minus1;
        uint32_t lod_level;
        uint32_t group_child_index;
        uint32_t groupID;
    };

    struct Group
    {

    };

    struct Node
    {
        Scene::AABB bound_box_;

    };

    struct TessellationTable
    {
        HAL::HALBuffer* vertices{ nullptr };
        HAL::HALBuffer* triangles{ nullptr };

    };

    namespace
    {
        struct ClusterInfo
        {
            uint32_t instanceID;
            uint32_t clusterID;
        };

        struct TraversalInfo
        {
            uint32_t instanceID;
            uint32_t packedNode;
        };
#ifdef  aaa //raytracing
        struct BlasBuildInfo
        {
            uint32_t cluster_ref_count; //the number of CLAS this blas reference
            uint32_t cluster_ref_stride; //stride of array(typically 8 for 64bit)
            uint64_t cluster_reference; //start address of the array
        };
#endif
    }

    class GPUSceneProxy
    {
    public:
        explicit GPUSceneProxy(Scene::WorldScene* scene);
        virtual ~GPUSceneProxy() { UnInit(); };

        virtual void Init(SceneProxy* scene);
        virtual void UnInit() {}
        FORCE_INLINE auto GetCurrentFrameIndex() const { return curr_frame_index_; }
        FORCE_INLINE auto GetPrimitivesNum() const { return 0u; }
        /*extract shader parameters*/
        void GetSceneShaderParameters(); //todo like unreal engine FGPUSceneResourceParameters GetShaderParameters(FRDGBuilder& GraphBuilder) const;
    private:
        GPUDataUploader* GetPrimitiveSparseUploader();
        GPUDataUploader* GetInstanceUploader();
        //predict curr frame gpu data size from the upload system loader
        auto PredictCurrFramePrimitiveSize() const;
        auto PredictCurrFrameInstanceSize() const;
        void CacheStaticDrawCommands(void); //todo draw command cache in other place
        
    private:
        HAL::HALBuffer* primitive_device_buffer_{ nullptr }; //geometry
        //primitive upload stage buffer
        HAL::HALBuffer* primitive_stage_buffer_{ nullptr };
        GPUDataUploader* primitive_uploader_{ nullptr };
        HAL::HALBuffer* instance_device_buffer_{ nullptr }; //instance 
        //instance data upload stage buffer
        HAL::HALBuffer* instance_stage_buffer_{ nullptr };
        GPUDataUploader* instance_uploader_{ nullptr };
        HAL::HALBuffer* light_stage_buffer_{ nullptr };
        HAL::HALBuffer* light_device_buffer_{ nullptr };
        GPUDataUploader* light_uploader_{ nullptr };

        //static primitive mesh cache 
        Vector<StaticPrimitiveMeshInfo> static_mesh_batches_;
        Map<uint32_t, Vector<StaticPrimitiveMeshDrawInfo>> pass_static_mesh_command_infos_;
        Vector<PrimitiveBatchDrawCommand>   static_mesh_commands_;
        size_type   curr_frame_index_{ 0u };
    private:
        /****************************************************************************************/
        /*                        CLAS data members                                             */
        /****************************************************************************************/
        GPUSceneStreaming stream_;
        TessellationTable tessellation_tbl_;
        HAL::HALBuffer* persistent_memory_{ nullptr };
        HAL::HALBuffer* persistent_memory_used_bits_{ nullptr };

        /*buffer for clas scratch buffer, free range and bin*/
        HAL::HALBuffer* temporary_buffer_{ nullptr };

#if aaaa //raytracing member
#endif


    };

    struct GPUStreamingStats
    {

    };

    class GPUSceneStreaming
    {
    public:
        void Init();
        void UnInit();
        void Reset();
        void PreRender(HAL::HALCommandContext& cmd_buffer);
        void PostRender(HAL::HALCommandContext& cmd_buffer);

        /** pre traversal scene*/
        void PreTraversal(HAL::HALCommandContext& cmd_buffer); //build nvidia clas accerlerate 
        void PostTraveral(HAL::HALCommandContext& cmd_buffer); //unload noused 
    private:
        size_type   persistent_geometry_size_{ 0u };
        size_type   operation_size_{ 0u };
        size_type   clas_operation_size_{ 0u };
        GPUStreamingStats   streaming_stat_;
    };
}
