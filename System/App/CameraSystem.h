#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/SimpleEntitySystem.h"
#include "Scene/Primitive.h"

namespace Shard::System::Camera
{

    struct ECSCameraComponent
    {
        enum class EType : uint32_t 
        {
            ePerspective = 0x1,
            eOrthographic = 0x2,
            eReverseZ = 0x100, //reserve-z for depth precision
        };

        EType    type_{ EType::ePerspective };


        float3    center_{ 0.f, 0.f, 0.f };
        float3    eye_{ 0.f, 0.f, 1.f };
        float3    up_{ 0.f, 1.f, 0.f };

        //world to camera transform matrix
        mat4    view_;
        mat4    inv_view_;

        float    znear_{ 1.f };
        float    zfar_{ 5000.f };
        float    focal_length_{ 1.f };
        float    fov_{ 60.f };
        float    aperture_size_{ 0.f };
        Rect    view_port_;

        //camera to film transform matrix
        mat4    proj_;
        mat4    inv_proj_; 
        
        bool IsPerspective()const {
            return Utils::HasAnyFlags(type_, EType::ePerspective);
        }
        bool IsReverseZ()const {
            return Utils::HasAnyFlags(type_, EType::eReverseZ);
        }
        void ComputeMatrix(bool compute_proj = false);
        void Update(mat3 rotation);
    };

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