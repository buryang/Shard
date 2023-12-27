#pragma once
#include "Utils/SimpleEntitySystem.h"
#include "Utils/SimpleEntitySystemPrimitive.h"

namespace Shard::System::Animation
{
        struct ECSSkeletalMeshComponent
        {
                Entity        root_bone_;
                Vector<mat4>        skin_matrix_;
        };

        struct ECSArmatureComponent
        {

        };

        struct AnimationClip
        {
                enum class EWarpMode
                {
                        eOnce, //play once,reset to beginning
                        eLoop,
                        ePingPong,
                        eClampForever, //play once, then repeat last frame
                };
        };

        struct ECSAnimationComponent
        {
                vec2        duration_{ 0.f, 0.f }; //animation start&end
                float        time_; //whole time
                float        speed_{ 0.f };
                float        last_update_time_{ 0.f };
                union {
                        uint32_t        packed_flags_{ 0u };
                        struct
                        {
                                uint32_t        looped_ : 1;
                                uint32_t        playing_ : 1;
                        };
                };

                void AddClip(AnimationClip&& clip);
        };

        struct ECSMorphAnimationCompoent
        {
                enum {
                        MAX_MORPH_COUNT = 32u,
                };
                Array<float, MAX_MORPH_COUNT>        morph_weights_;
        };

        class MINIT_API ProceduralAnimationSystem : public Utils::ECSSystem
        {

        };
}
