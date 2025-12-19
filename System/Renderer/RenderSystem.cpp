#include "Utils/CommonUtils.h"
#include "Utils/SimpleCoro.h"
#include "Core/EngineGlobalParams.h"
#include "Scene/Scene.h"
#include "Graphics/FXRModule/DebugView/FXRDebugView.h"
#include "RenderPipeline.h"
#include "GPUUploadSystem.h"
#include "RenderSystem.h"



namespace Shard::Renderer {

    /*render core type the render system used*/
    REGIST_PARAM_TYPE(FLOAT, RENDER_TARGET_FPS, 30);
    REGIST_PARAM_TYPE(BOOL, RENDER_SKIP_FRAME, false);
    REGIST_PARAM_TYPE(BOOL, RENDER_FIXED_FPS, false);
    REGIST_PARAM_TYPE(BOOL, RENDER_DELAY_DEL, false);//whether delay render delete
	REGIST_PARAM_TYPE(UINT, RENDER_PIPLINE_TYPE, ERenderPipelineType::eDefault); //currently only support 3D render pipline
    
    REGIST_PARAM_TYPE(BOOL, RENDER_DISTANCE_FADE_TRANSITION_ENABLED, false); //distance base object fade out 

    void RenderSystem::Init()
    {
#ifdef DEVELOP_DEBUG_TOOLS
        FXR::RtDebugViewRender::Init();
#endif
        const auto render_type = static_cast<ERenderPiplineType>(GET_PARAM_TYPE_VAL(UINT, RENDER_PIPLINE_TYPE));
        switch (render_type) {
		case ERenderPipelineType::eDefault:
		case ERenderPipelineType::eHDRP_3D:
			Runtime::RuntimeHDRPRender3D::RenderCfg render_cfg;
			//collect render configure
        	render_ = new Runtime::RuntimeHDRPRender3D;
            break;
        default:
            LOG(ERROR) << "render type is not supported by system";
        }

		render_->Init();
    }

    void RenderSystem::UnInit()
    {
#ifdef DEVELOP_DEBUG_TOOLS
        FXR::RtDebugViewRender::UnInit();
#endif
        for (auto& [_, generator] : batch_generator_) {
            generator->UnInit();
        }

		if (nullptr != render_)
		{
			delete render_;
		}
    }

    void RenderSystem::Update(ECSSystemUpdateContext& ctx)
    {
        auto* render_group = reinterpret_cast<Scene::WorldScene*>(ctx.admin_)->Group<ECSStaticMeshComponent>();
        assert(render_group != nullptr);
        
        //update renderable components 
        for (auto& [_, generator] : batch_generator_) {
            generator->Update();
        }

        //todo render
        //use render filtering setting to get MeshBatch
    }

    void RenderSystem::SetWindow(Utils::WindowHandle win)
    {
    }

    Runtime::DrawMeshBatchGeneratorInterface* RenderSystem::GetBatchGenerator(uint32_t id)
    {
        assert(batch_generator_.find(id) != batch_generator_.end());
        return batch_generator_[id];
    }

    void RenderSystem::ReloadRender()
    {
    }

    void RenderSystem::OnWindowResize(uint32_t width, uint32_t height)
    {
    }

    static void InitViewRender(FXR::ViewRender& render, const SceneViewInstance& view)
    {

    }

    static void UpdateViewRender(FXR::ViewRender& render, const SceneViewInstance& view)
    {

    }

    void RenderSystem::InitViewRenders(const SceneView& view)
    {
        view_renders_.resize(view.Count());
        auto index = 0u;
        for (auto iter = view.CBegin(); iter != view.CEnd(); ++iter)
        {
            auto& render = view_renders_[index++];
            InitViewRender(render, **iter);
            //set lod proxy
            render.lod_proxy_view_ = (*iter)->IsSecondaryView() ? &view_renders_[0] : &render;  
        }
    }

    void RenderSystem::UpdateViewRenders(const SceneView& view)
    {
        assert(view_renders_.size() == view.Count());
        for (auto viter = view.CBegin(), auto riter = view_renders_.begin(); viter != view.CEnd(); ++viter, ++riter)
        {
            UpdateViewRender(*riter, **viter);
        }
    }

    RenderSystem::~RenderSystem()
    {
        if (nullptr == render_) {
            delete render_;
            render_ = nullptr; 
        }
    }

    void RenderSystem::GPUSceneProxy::Update()
    {
        //use job system to update world
        //1.update static mesh primitives

        //2.update dynamic mesh primitives

        //3. update light 

        //4. do CPU culling
    }

    //todo move the logic to init
    GPUDataUploader* RenderSystem::GPUSceneProxy::GetPrimitiveSparseUploader()
    {
        auto* upload_system = world->GetSystem<GPUUploadSystem>();
        assert(upload_system != nullptr);
        auto* uploader = upload_system->AccquireDataUploader(EUploaderType::eSparse);
        return uploader;
    }

    GPUDataUploader* RenderSystem::GPUSceneProxy::GetInstanceUploader()
    {
        auto* upload_system = world->GetSystem<GPUUploadSystem>();
        assert(upload_system != nullptr);
        auto* uploader = upload_system->AccquireDataUploader(EUploaderType::eSparse);
        return nullptr;
    }

    auto RenderSystem::GPUSceneProxy::PredictCurrFramePrimitiveSize() const
    {
        const auto prev_pritimive_size = const_cast<GPUSceneProxy*>(this)->GetPrimitiveSparseUploader()->GetMaxOccupySize();
        const auto primitive_size_limits = GET_PARAM_TYPE_VAL(UINT, RENDER_SYSTEM_PRIMITIVE_LIMITS);
        return Utils::Min((size_type)(prev_pritimive_size * 1.2), primitive_size_limits); 
    }

    auto RenderSystem::GPUSceneProxy::PredictCurrFrameInstanceSize() const
    {
        const auto prev_instance_size = const_cast<GPUSceneProxy*>(this)->GetInstanceUploader()->GetMaxOccupySize();
        const auto instance_size_limits = GET_PARAM_TYPE_VAL(UINT, RENDER_SYSTEM_INSTANCE_LIMITS);
        return Utils::Min((size_type)(prev_instance_size * 1.3), instance_size_limits);
    }



}