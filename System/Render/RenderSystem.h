#pragma once
#include "Core/EngineGlobalParams.h"
#include "Utils/Algorithm.h"
#include "Utils/Platform.h"
#include "Utils/SimpleEntitySystem.h"
#include "Utils/SimpleEntitySystemPrimitive.h"
#include "Runtime/RuntimeSceneRenderer.h"
#include "Runtime/RuntimeMeshData.h"
#include "Runtime/RuntimeCommandIR.h"
#include "SceneProxy/SceneProxy.h"

/*render thread main entrance and logic*/
namespace Shard::Render
{
    namespace Runtime {
        class DrawMeshBatchGeneratorInterface;
    }

    class MINIT_API RenderSystem : public Utils::ECSSystem
    {
    public:
        void Init() override;
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
    private:
        
        struct StaticPrimitiveMeshInfo
        {
            uint32_t    id_handle_;
            //hal buffer and offset
            uint32_t    offset_{ 0u };
        };

        struct StaticPrimitiveMeshDrawInfo
        {
            uint32_t    id_handle_;
            uint32_t    command_index_{ 0u };
            uint32_t    flags_{ 0u };
        };

        Utils::WindowHandle window_;
        Runtime::RuntimeRenderBase* render_{ nullptr };
        GPUSceneProxy*  gpu_scene_{ nullptr };
        //renderable groups
        Utils::ECSComponentGroupBase*    opaque_{ nullptr };
        Utils::ECSComponentGroupBase*    transparent_{ nullptr };

        //mesh batch factroy
        Map<uint32_t, Runtime::DrawMeshBatchGeneratorInterface*> batch_generator_;

        //mesh update context //todo dynamic and static
        Vector<Runtime::MeshBatch>    mesh_batches_;
        Vector<Runtime::MeshBatchRangeProperty> mesh_batch_ranges_;

        //static mesh command buffer
        Runtime::DrawWorkCommandIRBins   static_mesh_command_buckets_;
        
        //light update context
        //todo different viewinfo like unreal

    };
}