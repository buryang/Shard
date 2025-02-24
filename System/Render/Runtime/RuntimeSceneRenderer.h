#pragma once
#include "Utils/CommonUtils.h"

namespace Shard::Render
{
    namespace Runtime
    {
        class RuntimeViewContext
        {
        public:
            using Ptr =    RuntimeViewContext*;
        private:
            vec3    pos_;
            vec3    view_;
        };

        class RuntimeRenderBase
        {
        public:
            virtual void Render() = 0;
        };

        class RuntimeSceneRenderBase : public RuntimeRenderBase
        {
        public:
            explicit RuntimeSceneRenderBase(Scene::WorldScene* scene) :scene_(scene)
            {

            }
            void Render()override {
                RenderScene();
            }
            virtual ~RuntimeSceneRenderBase() = default;
            FORCE_INLINE bool IsScenceValid()const {
                return scene_ != nullptr;
            }
            FORCE_INLINE void SetScence(Scene::WorldScene*    scene) {
                scene_ = scene;
            }
        protected:
            virtual void RenderScene() = 0; //change this interface
        protected:
            Scene::WorldScene*    scene_{ nullptr };
            //render view context
            SmallVector<RuntimeViewContext*>        views_;
        };
    }
}
