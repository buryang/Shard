#pragma once
#include "Core/EngineGlobalParams.h"
#include "Utils/Algorithm.h"
#include "Utils/Platform.h"
#include "Utils/SimpleEntitySystem.h"
#include "Utils/SimpleEntitySystemPrimitive.h"
#include "Runtime/RuntimeMeshData.h"
#include "Runtime/RuntimeCommandIR.h"
#include "SceneProxy/SceneProxy.h"
#include "SceneProxy/SceneView.h"
#include "RenderCommon.h"
#include "RenderPipeline.h"


/*render thread main entrance and logic*/
namespace Shard::Renderer
{
    namespace Runtime {
        class DrawMeshBatchGeneratorInterface;
    }

    /*now only support mirror/stereo camera multi-views */
    static constexpr auto max_view_renders = 2u; 

    /* render system work with some related subsytem, for example gpuuploader/vttexure etc 
     * , scene proxy and a list of fxr modules to finish render jobs
     */
    class MINIT_API RenderSystem : public Utils::ECSSystem
    {
    public:
        void Init(World* world) override;
        void UnInit() override;
        void Update(Utils::ECSSystemUpdateContext& ctx) override;
        void SetWindow(Utils::WindowHandle win);
        /*do the mesh regist like unity, todo*/
        uint32_t RegisterMesh();
        uint32_t RegisterMaterial();

        Runtime::DrawMeshBatchGeneratorInterface* GetBatchGenerator(uint32_t id);
        ~RenderSystem();
    private:
        RenderSystem() = default;
        DISALLOW_COPY_AND_ASSIGN(RenderSystem);
        void ReloadRender();
        void OnWindowResize(uint32_t width, uint32_t height);
        void InitViewRenders(const SceneView& view);
        void UpdateViewRenders(const SceneView& view);
    private:
        
        struct StaticPrimitiveMeshInfo
        {
            uint32_t    id_handle_;
            //hal buffer and offset
            uint32_t    offset_{ 0u };
        };

        struct StaticPrimitiveMeshDrawInfo
        {
			uint32_t    id_handle_{ ~0u };
            uint32_t    command_index_{ 0u };
            uint32_t    flags_{ 0u };
        };

        Utils::WindowHandle window_;

        SceneProxy* scene_proxy_{ nullptr };
        SceneView view_base_; /*view base generated from camera component*/
        /*each view has its own render context */
        Array<FXR::ViewRender, max_view_renders> view_renders_;
        /*view renderpipline for the view to be rendered */
        RenderPipelineBase*   render_pipeline_;

        uint64_t frame_index_{ 0u };
        //renderable groups
        Utils::ECSComponentGroupBase*    opaque_{ nullptr };
        Utils::ECSComponentGroupBase*    transparent_{ nullptr };

        //mesh batch factory
        Map<uint32_t, Runtime::DrawMeshBatchGeneratorInterface*> batch_generator_;

        //mesh update context //todo dynamic and static
        RenderArray<Runtime::MeshBatch>    mesh_batches_;
        RenderArray<Runtime::MeshBatchRangeProperty> mesh_batch_ranges_;

        //static mesh command buffer
        Runtime::DrawWorkCommandIRBins   static_mesh_command_buckets_;
        
        //light update context
        //todo different viewinfo like unreal
        
        //scene draw command buffer ping-pong
        //sync render/hal/display jobs

    };
}