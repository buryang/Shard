#pragma once
#include "Utils/Algorithm.h"
#include "Utils/Platform.h"
#include "Utils/SimpleEntitySystem.h"
#include "Scene/Scene.h"

/*render thread main entrance and logic*/
namespace Shard::Renderer
{
    class MINIT_API RenderSystem : public Utils::ECSSystem
    {
    public:
        void Init() override;
        void UnInit() override;
        void Update(float dt) override;
        void SetWindow(Utils::WindowHandle win);
        void ReloadRender(); 
    private:
        RenderSystem() = default;
        DISALLOW_COPY_AND_ASSIGN(RenderSystem);
    };
}