#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Platform.h"
#include "Utils/Memory.h"
#include "Utils/SimpleJobSystem.h"
#include "Scene/Scene.h"
#include "System/System.h"
#include "Delegate/Delegate/MulticastDelegate.h"

namespace Shard
{
    namespace Runtime
    {
        namespace SI = System::Input;

        class MINIT_API Engine
        {
        public:
            virtual void Init(Utils::WindowHandle win) = 0;
            virtual void UnInit() = 0;
            virtual void FixedUpdate() = 0;
            virtual void Update(float dt) = 0;
            virtual void LateUpdate() = 0;
            void Run();
            FORCE_INLINE void Suspend() {
                is_suspended_ = true;
            }
            FORCE_INLINE void Resume() {
                is_suspended_ = false;
            }
            FORCE_INLINE bool IsSuspended() const {
                return is_suspended_;
            }
            virtual ~Engine() = default;
            //export system for message loop
            SI::InputSystem* GetInputSystem();
        protected:
            Scene::WorldScene*  world_{ nullptr }; //current active world
            Scene::WorldSceneManager    world_manager_; //all world manager
            SI::InputSystem*    input_system_{ nullptr };
            void ResetTimeStatistics();
            //Render::
        private:
            Utils::JobEntry*    prev_render_{ nullptr };
            Utils::WindowHandle window_{ nullptr };
            bool    is_suspended_{ false };
        protected:
            double  delta_time_{ 0.f };
            float   acc_delta_time_{ 0.f };
            float   average_delta_time_{ 0.f };
            size_type   curr_frame_{ 0u };
            //global internal allocator for whole engine
            Utils::StackAllocator<void> frame_transient_alloc_;
            Utils::LinearAllocator<void>    persist_alloc_;
        };

#ifdef APPLICATION_RUNTIME
        class MINIT_API ApplicationEngine : public Engine
        {
        public:
            enum class EStageType
            {
                eNone,
                eLoad, //load screen/load resource
                eRun, //app run
                eUnLoad,
            };
        public:
            void Init(Utils::WindowHandle win) override;
            void UnInit() override;
            void FixedUpdate() override;
            void Update(float dt) override;
            void LateUpdate() override;
            void SetCurrStage(EStageType stage) {
                assert(stage != EStageType::eNone);
                on_stage_change_(curr_stage_, stage);
                curr_stage_ = stage;
            }
            EStageType GetCurrStage()const {
                return curr_stage_;
            }
            auto& OnStageChange() {
                return on_stage_change_;
            }
        private:
            DISALLOW_COPY_AND_ASSIGN(ApplicationEngine);
        private:
            EStageType    curr_stage_{ EStageType::eNone };
            DelegateLib::MulticastDelegate<void(EStageType, EStageType)>    on_stage_change_;
        };
#elif defined(EDITOR_RUNTIME)
        class MINIT_API EditorEngine : public Engine
        {
            //todo 
        };
#endif
    }
}