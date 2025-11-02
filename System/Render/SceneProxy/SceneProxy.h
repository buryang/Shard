#pragma once

#include <nvclusterlod/nvclusterlod_hierarchy_storage.hpp>
#include <nvclusterlod/nvclusterlod_mesh_storage.hpp>
#include <nvclusterlod/nvclusterlod_cache.hpp>

#include "Core/EngineGlobalParams.h"
#include "Utils/CommonUtils.h"
#include "HAL/HALCommandIR.h"
//#include "Renderer/GPUMem/GPUPersistentMemoryAllocator.h"
#include "Scene/Scene.h"
#include "../GPUUploadSystem.h"
#include "SceneStreaming.h"

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

    //configure for clas/group 
    REGIST_PARAM_TYPE(UINT, RENDER_SYSTEM_CLAS_VERTICES, 64);
    REGIST_PARAM_TYPE(UINT, RENDER_SYSTEM_CLAS_TRIANGLES, 64);
    REGIST_PARAM_TYPE(UINT, RENDER_SYSTEM_CLAS_GROUP_SIZE, 32);



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
        //check whether special primitive existed
        bool IsHairPrimitveExsited()const;
        bool IsFoliagePrimitveExsited()const;
        bool IsVolumePrimitveExsited()const;

        size_type GetCLASGeometryCount()const;
        void Tick(float delta_time);

        template<typename primitive_func>
        void EnumeratePrimitives(primitive_func&& func)
        {
            //todo
        }

        template<typename light_func>
        void EnumerateLights(light_func&& func)
        {
            //todo
        }
    private:
        GPUSceneProxy* gpu_scene_{ nullptr };
        //todo
        //Vector<> geometries_;
        //Vector<> clas_geometries_;
        bool with_hair_primitves_;
        bool with_foliage_primitves_;
        bool with_volume_primitves_;
    };

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
        HAL::HALBuffer* GetPrimitiveBufferCPU() const { return primitive_stage_buffer_; }
        HAL::HALBuffer* GetInstanceBufferCPU() const { return instance_stage_buffer_; }
        HAL::HALBuffer* GetPrimitiveBufferGPU() const { return primitive_device_buffer_; }
        HAL::HALBuffer* GetInstanceBufferGPU() const { return instance_device_buffer_; }
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
        /*                        CLAS data members, for loading nvclusterlod asset             */
        /*                                                  */
        /****************************************************************************************/
        GPUSceneStreaming stream_;

        struct GPUData
        {
            /*persisitent memory hold clusters*/
            HAL::HALBuffer* persistent_memory_{ nullptr };
            HAL::HALBuffer* persistent_memory_used_bits_{ nullptr };

            HAL::HALBuffer* vertices_{ nullptr };
            HAL::HALBuffer* triangles_{ nullptr };

            /*buffer for clas scratch buffer, free range and bin*/
            HAL::HALBuffer* temporary_buffer_{ nullptr };

            /*instance traversal buffers*/
            HAL::HALBuffer* traversal_info_buffer_{ nullptr };
            /*traversal result, cluster need to load to render*/
            HAL::HALBuffer* render_cluster_info_buffer_{ nullptr };
#if 0 //raytracing member
#endif
        };
        
        //bindless buffer resource index and resource size
        struct GPUDataBindlessInfo
        {
            uint32_t persistent_memory_index_{ 0u };
            uint32_t persistent_memory_used_bits_index_{ 0u };
            uint32_t traversal_info_buffer_{ 0u };
            uint32_t traversal_info_size_{ 0u };
            uint32_t render_cluster_info_index_{ 0u };
            uint32_t render_cluster_info_size_{ 0u };
            uint32_t extent_payload_index0_{ 0u };
            uint32_t extent_payload_index1_{ 0u };
        };
        GPUData gpu_buffers_;
        GPUDataBindlessInfo bindless_index_;

    };

}
