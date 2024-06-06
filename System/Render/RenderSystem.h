#pragma once
#include "Core/EngineGlobalParams.h"
#include "Utils/Algorithm.h"
#include "Utils/Platform.h"
#include "Utils/SimpleEntitySystem.h"
#include "Utils/SimpleEntitySystemPrimitive.h"
#include "Runtime/RuntimeSceneRenderer.h"

/*render thread main entrance and logic*/
namespace Shard::Render
{
    class MINIT_API RenderSystem : public Utils::ECSSystem
    {
    public:
        void Init() override;
        void UnInit() override;
        void Update(ECSSystemUpdateContext& ctx) override;
        void SetWindow(Utils::WindowHandle win);
        ~RenderSystem();
    private:
        RenderSystem() = default;
        DISALLOW_COPY_AND_ASSIGN(RenderSystem);
        void ReloadRender();
        void OnWindowResize(uint32_t width, uint32_t height);
    private:
        Utils::WindowHandle window_;
        Runtime::RuntimeRenderBase* render_{ nullptr };
    };
}