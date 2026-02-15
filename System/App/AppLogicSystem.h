#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/SimpleEntitySystem.h"
#include "Scene/Primitive.h"
#include "AppComon.h"

/**
 * application logic system here, for example editor/fps game/others etc
 * current just using fpsgame as a example for mini-engine test
 */

namespace Shard::System::App
{
    class MINIT_API FPSCameraMovementSystem : public Utils::ECSSystem
    {
    public:
        void Init() override;
        void UnInit() override;
        void Update(Utils::ECSSystemUpdateContext& ctx) override;
    protected:
        float direction_scale_{ 1.f };
        ECSCameraComponent* camera_{ nullptr }; //camera singleton
    };
}