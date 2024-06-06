#include "Utils/CommonUtils.h"
#include "Core/EngineGlobalParams.h"
#include "Scene/Scene.h"
#include "Graphics/Effect/DebugView/EffectDebugView.h"
#include "System/Render/RenderSystem.h"



namespace Shard::Render {

    /*render core type the render system used*/
    REGIST_PARAM_TYPE(UINT, RENDER_CORE_TYPE, 0u);
    REGIST_PARAM_TYPE(FLOAT, RENDER_TARGET_FPS, 30);
    REGIST_PARAM_TYPE(BOOL, RENDER_SKIP_FRAME, false);
    REGIST_PARAM_TYPE(BOOL, RENDER_FIXED_FPS, false);
    REGIST_PARAM_TYPE(BOOL, RENDER_DELAY_DEL, false);//whether delay render delete

    void RenderSystem::Init()
    {
#ifdef DEVELOP_DEBUG_TOOLS
        Effect::RtDebugViewRender::Init();
#endif
        const auto render_type = GET_PARAM_TYPE_VAL(UINT, RENDER_CORE_TYPE);
        switch (render_type) {
        case xx:
            render_ = nullptr;
            break;
        case tt:
            render_ = nullptr;
            break;
        default:
            LOG(ERROR) << "render type is not supported by system";
        }
    }

    void RenderSystem::UnInit()
    {
#ifdef DEVELOP_DEBUG_TOOLS
        Effect::RtDebugViewRender::UnInit();
#endif
    }

    void RenderSystem::Update(ECSSystemUpdateContext& ctx)
    {
        auto* render_group = reinterpret_cast<Scene::WorldScene*>(ctx.admin_)->Group<ECSStaticMeshComponent>();
        assert(render_group != nullptr);
        //todo

    }

    void RenderSystem::SetWindow(Utils::WindowHandle win)
    {
    }

    void RenderSystem::ReloadRender()
    {
    }

    void RenderSystem::OnWindowResize(uint32_t width, uint32_t height)
    {
    }

    RenderSystem::~RenderSystem()
    {
        if (nullptr == render_) {
            delete render_;
            render_ = nullptr;
        }
    }

}