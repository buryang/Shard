#pragma once

#include <nvclusterlod/nvclusterlod_hierarchy_storage.hpp>
#include <nvclusterlod/nvclusterlod_mesh_storage.hpp>
#include <nvclusterlod/nvclusterlod_cache.hpp>

#include "Core/EngineGlobalParams.h"
#include "Utils/CommonUtils.h"
#include "Utils/SimpleCoro.h"
#include "Utils/Memory.h"
#include "Utils/SimpleEntitySystem.h"
#include "HAL/HALCommandIR.h"
#include "Graphics/Render/RenderShader.h"
//#include "Renderer/GPUMem/GPUPersistentMemoryAllocator.h"
#include "Scene/Scene.h"
#include "../GPUUploadSystem.h"
#include "SceneStreaming.h"
#include "SceneView.h"

namespace Shard::Renderer
{
    class GPUSceneStreaming;
    class GPUSceneProxy;
    class GPUDataUploader;

    using Utils::Entity;

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

    struct ECSExtractedVFXSystem
    {

    };

    struct ECSExtractedPrimitve
    {

        //share primive & material handle with world scene proxy
    };

    struct ECSExtractedLight
    {

    };

    struct ECSExtractedCamera
    {

    };

    struct ECSExtractedHairPrimitve
    {

    };

    struct ECSExtractedFoliagePrimitve
    {
    };

    struct ECSExtractedVolumePrimitve
    {
    };


    //primitive draw command
    struct PrimitiveBatchDrawCommand
    {
        //todo pso binding
        //todo vector
    };

    //declare global GPU scene unifrom buffer
    BEGIN_SHADER_PARAMETER_DEF(GPUSceneUniformBuffer)
    END_SHADER_PARAMETER_DEF()


    class SceneProxy : public Utils::ECSAdmin<Allocator = TLSScalablePoolAllocatorInstance<uint8_t, POOL_RENDER_SYSTEM_ID> > //todo
    {
    public:
        SceneProxy() = default;

        void Init(Scene::WorldScene* world);
        void UnInit();

        //extract WorldScene renderable entity to sceneproxy
        void OnRenderBegin();
        //remove entities who are temporary
        void OnRenderEnd();

        /*prepare scene for view-related data*/
        JobHandle AsyncPrepareForViews(const SceneView& view);

        //add logic sync with ecs world scene
        JobHandle AsyncApplyWorldUpdates();
        //add logic sync with gpu proxy to command buffer(hal layer multithread backend update)
        JobHandle BuildGPUSceneProxyUpdateCommands(HAL::HALCommandContext& cmd_buffer) const;

        GPUSceneProxy* GetGPUSceneProxy() const { return gpu_scene_; }


        /*whether scene supported CLAS scene*/
        FORCE_INLINE bool IsCLASSupported()const;
        //check whether special primitive existed
        FORCE_INLINE bool IsHairPrimitveExsited() const { return GetComponentRepos<ECSHairPrimitveProxy>().IsEmpty(); }
        FORCE_INLINE bool IsFoliagePrimitveExsited() const { return true; /*todo*/ }
        FORCE_INLINE bool IsVolumePrimitveExsited()const { return true; /*todo*/ }
        FORCE_INLINE bool IsVolumeCloudPrimitiveExsited() const { return true; /*todo*/ }
        FORCE_INLINE bool IsLocalVolumePrimitveExsited() const { return true; /*todo*/ }
        FORCE_INLINE bool IsSkyAtmospherePrimitiveExsited() const { return true; /*todo*/ }
        FORCE_INLINE size_type GetCLASGeometryCount()const;
        
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
        DISALLOW_COPY_AND_ASSIGN(SceneProxy);
        Utils::Coro<> AsyncApplyPrimitiveUpdatesFromWorld();
        Utils::Coro<> AsyncApplyLightUpdatesFromWorld();

        //other detail primitive update, for example wind, water, foliage etc.
        void AddHairStrands();
        void RemoveHairStrands();
        void AddLocalFogVolume();
        void RemoveLocalFogVolume();
    private:
        GPUSceneProxy*  gpu_scene_{ nullptr };

        //scene entity mapping to render world entity
        HashMap<Entity, Entity> entity_scene_to_proxy_;
        
        //do not need primitive proxy
        //just query all primitive from ecs after app logic finalized
        //build some essential struct for primitive and lights here

        //grok recomend
        //1. Query visible entities.
        //2. Compute view - space data or fill instance buffers.
        //3. Record commands.

        //it's a data access overlap problem between render & game system
        //current the solution is double-data buffer, one for read , one for write
        //...


        //todo static/dynamic primitves
        //other procedual meshes, like water/fluid/vfx particles

        //todo packed scene primitives and instances
        //we need to implement a gather system to collect scene geometry from ecs system
        //SceneProxyArray geometries_;
        //SceneProxyArray clas_geometries_;
    };

    class GPUSceneProxy
    {
    public:
        explicit GPUSceneProxy(Scene::WorldScene* scene);
        virtual ~GPUSceneProxy() { UnInit(); };

        virtual void Init(SceneProxy* scene);
        virtual void UnInit() {}

        //upload view-related primitves to gpu scene
        void StreamViewRelatedPrimitives(Render::RenderGraph& graph, SceneView& view);

        FORCE_INLINE auto GetPrimitivesNum() const { return 0u; }
        /*extract shader parameters*/
        void SetupGPUSceneShaderParameters(GPUSceneUniformBuffer& scene_param)const; //todo like unreal engine FGPUSceneResourceParameters GetShaderParameters(FRDGBuilder& GraphBuilder) const;
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
        //gpuscene uniform buffer here
        GPUSceneUniformBuffer   gpu_scene_ubo_;

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

        //todo lights frustum-voxel grid /CPU or GPU ?
        //using light grid for culling like unreal engine megalights( for mobile some engine use tiled grid lights)


        //static primitive mesh cache 
        Vector<StaticPrimitiveMeshInfo> static_mesh_batches_;
        Map<uint32_t, Vector<StaticPrimitiveMeshDrawInfo>> pass_static_mesh_command_infos_;
        Vector<PrimitiveBatchDrawCommand>   static_mesh_commands_;
    private:
        /****************************************************************************************/
        /*                        CLAS data members, for loading nvclusterlod asset             */
        /*                                                                                      */
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
        //uniform buffer store bindless information
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
