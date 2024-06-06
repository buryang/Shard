#include "Utils/Timer.h"
#include "Utils/SimpleJobSystem.h"
#include "Runtime/RuntimeEngineCore.h"
#include "Core/EngineGlobalParams.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace Shard
{
    namespace Runtime {

        //regist jobsystem configure
        REGIST_PARAM_TYPE(UINT, JOBSYS_GRP_COUNT, 0u);
        REGIST_PARAM_TYPE(UINT, JOBSYS_QUEUE_SIZE, 16u);
        REGIST_PARAM_TYPE(BOOL, JOBSYS_DEDICATE_CORE, false);

        //regist system configure
        REGIST_PARAM_TYPE(BOOL, SYSTEM_AFFINITY_AUTO, true);
        REGIST_PARAM_TYPE(UINT, SYSTEM_PhysX_AFFINITY, 0xFFFFFFFF);
        REGIST_PARAM_TYPE(UINT, SYSTEM_APP_AFFINITY, 0xFFFFFFFF);
        REGIST_PARAM_TYPE(UINT, SYSTEM_RENDER_AFFINITY, 0xFFFFFFFF);
        REGIST_PARAM_TYPE(UINT, SYSTEM_AUX_AFFINITY, 0xFFFFFFFF); //each auxility sub-task

        REGIST_PARAM_TYPE(BOOL, ENGINE_FIXED_FPS, true);
        REGIST_PARAM_TYPE(UINT, ENGINE_TARGET_FPS, 30);
        REGIST_PARAM_TYPE(BOOL, ENGINE_FRAME_SKIP, false);
        REGIST_PARAM_TYPE(UINT, ENGINE_VSYNC_COUNT, 0); //vertical syncs count 
        REGIST_PARAM_TYPE(FLOAT, ENGINE_TIME_AVER_EXP, 0.9);

        static void InitPlatformSpecAffinity()
        {
            const auto cpu_core = std::thread::hardware_concurrency();
            if (cpu_core == 2u) {
                //todo core index begin from 0?
                SET_PARAM_TYPE_VAL(UINT, SYSTEM_PhysX_AFFINITY, 1u);
                SET_PARAM_TYPE_VAL(UINT, SYSTEM_APP_AFFINITY, 1u);
                SET_PARAM_TYPE_VAL(UINT, SYSTEM_RENDER_AFFINITY, 2u);
            }
            else if (cpu_core >= 4) {
                SET_PARAM_TYPE_VAL(UINT, SYSTEM_PhysX_AFFINITY, 1u);
                SET_PARAM_TYPE_VAL(UINT, SYSTEM_APP_AFFINITY, 2u);
                SET_PARAM_TYPE_VAL(UINT, SYSTEM_RENDER_AFFINITY, 3u);
            }
        }

        void Engine::Init(Utils::WindowHandle win) {
            input_system_ = &SI::InputSystem::Instance();
            if (GET_PARAM_TYPE_VAL(BOOL, SYSTEM_AFFINITY_AUTO)) {
                InitPlatformSpecAffinity();
            }
            //init jobsystem
            auto cpu_core = std::thread::hardware_concurrency();
            if (auto cores = GET_PARAM_TYPE_VAL(UINT, JOBSYS_GRP_COUNT)) {
                cpu_core = cores;
            }
            auto queue_size = GET_PARAM_TYPE_VAL(UINT, JOBSYS_QUEUE_SIZE);
            Utils::SimpleJobSystem::Instance().Init(cpu_core, queue_size, GET_PARAM_TYPE_VAL(BOOL, JOBSYS_DEDICATE_CORE));

            //init work systems

        
        }
        void Engine::UnInit()
        {
            //uinit jobsystem
            Utils::SimpleJobSystem::Instance().UnInit();
        }

        void Engine::Run() 
        {
            const auto fixed_dt = 1.f / GET_PARAM_TYPE_VAL(UINT, ENGINE_TARGET_FPS);
            if (GET_PARAM_TYPE_VAL(BOOL, ENGINE_FIXED_FPS)) {
                if (acc_delta_time_ > 10.f) { //too long time delay
                    acc_delta_time_ = 0.f;
                }
        
                while (acc_delta_time_ >= fixed_dt) {
                    FixedUpdate();
                    acc_delta_time_ -= fixed_dt;
                }
            }
            else {
                FixedUpdate();
            }
            double delta_time{ 0.f };
            {
                Utils::ScopedTimer scoped_timer(delta_time);
                Update(delta_time_);
            }
            LateUpdate();//todo as unity says camera system need late update
            //update timer and governing the frame rate
            if (GET_PARAM_TYPE_VAL(BOOL, ENGINE_FIXED_FPS)) {
                if (delta_time < fixed_dt) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(uint32_t((fixed_dt - delta_time) * 1000)));
                    delta_time = fixed_dt;
                }
                else if (delta_time > fixed_dt && GET_PARAM_TYPE_VAL(BOOL, ENGINE_FRAME_SKIP)) {
                    //we must ¡°take our lumps¡± and wait for one more whole frame time to elapse
                    auto gap_time = fixed_dt;
                    while (gap_time < delta_time) {
                        gap_time += fixed_dt;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(uint32_t((gap_time - delta_time) * 1000)));
                    //todo delta time and frame index
                }
            }
            delta_time_ = delta_time;
            acc_delta_time_ += delta_time;
            const auto aver_exp = std::clamp(GET_PARAM_TYPE_VAL(FLOAT, ENGINE_TIME_AVER_EXP), 0.f, 1.f);
            average_delta_time_ = average_delta_time_ * aver_exp + delta_time * (1.f - aver_exp);
            LOG(INFO) << fmt::format("current %d frame execute time : %f, and average execute time: %f", curr_frame_, delta_time, average_delta_time_);
            curr_frame_++;
        }
        SI::InputSystem* Engine::GetInputSystem()
        {
            return input_system_;
        }

        void Engine::ResetTimeStatistics() 
        {
            acc_delta_time_ = 0.f;
            average_delta_time_ = 0.f;
            curr_frame_ = 0u;
        }

        namespace {
            constexpr float TIME_AVERAGE_EXPONENTIAL = 0.9;
        }

#ifdef APPLICATION_RUNTIME
        void ApplicationEngine::Init(Utils::WindowHandle win) {
            Engine::Init(win);
            const auto& execute_path = fs::current_path();

            //add system and components
        }

        void ApplicationEngine::UnInit() {

        }

        void ApplicationEngine::Update(float dt) {
            if (IsSuspended()) {
                ResetTimeStatistics();
                return;
            }

            //frame start
            input_system_->Tick(dt);

            world_manager_.Enumerate([dt](auto* world) {
                world->Update(dt); //todo
            });

            if (world_ == nullptr) {
                world_ = world_manager_.GetWorld();
            }
            //todo render active world
            //unreal redraw viewports


            //frame end
            input_system_->ClearFrameState();
        }

#elif defined(EDITOR_RUNTIME)
    #error "shard only support game runtime"
#endif
    }
}