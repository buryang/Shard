#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/SimpleEntitySystem.h"
#include "Utils/SimpleEntitySystemPrimitive.h"
#include "Scene/Primitive.h"
#include "Graphics/HAL/HALResources.h"

namespace Shard::System::PhysX
{
    //think about this for
    struct ECSWeatherComponent
    {
        enum class EFlags
        {
            eNone,

        };

        EFlags    flags_{ EFlags::eNone };
        vec3    sun_color_{ 0.f, 0.f, 0.f };
        vec3    sun_dir_{ 0.f, 1.f, 0.f };
        //other field
    };

    struct ECSParticleComponent
    {
        HAL::HALBuffer*    particle_buffer_{ nullptr };
        HAL::HALBuffer*    alive_particle_{ nullptr };
        HAL::HALBuffer*    dead_particle_{ nullptr };
    };

    struct ECSForceFieldComponent
    {
        //float    gravity_{ 9.8f };
    };

    struct ECSRigidBodyPhysXComponent
    {
        vec3    velocity_;
        vec3    angular_velocity_;
        float    mass_;
        enum
        {
            eNone,
            eBox = 0x1,
            eSphere = 0x2,
            eCapsule = 0x4,
        };
        uint32_t    flags_{ eNone };
        union
        {
            struct
            {
                vec3    half_;
            }box_;
            struct
            {
                float radius_;
            }sphere_;
            struct
            {
                float    half_height_;
                float    radius_;
            }capsule_;
        }collision_proxy_;
    }; 

    struct ECSSoftBodyPhysXComponent
    {
        float    mass_;
        AABB    aabb_;
        //todo
        //SmallVector<Utils::Entity>    bones_;
        //SmallVector<float>    bones_weights_;//todo
    };

    class MINIT_API PhysXDynamicUpdateSystem : public Utils::ECSSystem
    {
    public:
        void Init() override;
        void UnInit() override;
        void FixedUpdate() override;
        void Update(Utils::ECSSystemUpdateContext& ctx) override;
    public:
    };

    //particle system, maybe one for different indentity as wicked engine ?
    class MINIT_API PhysXEmitParticleSystem : public  Utils::ECSSystem
    {
    public:
        void Init() override;
        void UnInit() override;
        void FixedUpdate() override;
        void Update(Utils::ECSSystemUpdateContext& ctx) override;
    };

    class MINIT_API PhysXHairParticleSystem : public Utils::ECSSystem
    {
    public:
        void Init() override;
        void UnInit() override;
        void FixedUpdate() override;
        void Update(Utils::ECSSystemUpdateContext& ctx) override;
    };

}