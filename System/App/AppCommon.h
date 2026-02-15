#pragma once

#include "Utils/CommonUtils.h"

namespace Shard::System::App
{
    /*use right-hand coordinate */
    struct ECSCameraComponent
    {
        enum class EType : uint8_t
        {
            ePerspective = 0x1,
            eOrthographic = 0x2,
            eReverseZ = 0x10, //reserve-z for depth precision
            ePerspectiveR = ePerspective | eReverseZ,
            eOrthographicR = eOrthographic | eReverseZ,
        };

        EType   type_{ EType::ePerspective };

        float3  center_{ 0.f, 0.f, 0.f };
        float3  eye_{ 0.f, 0.f, 1.f };
        float3  up_{ 0.f, 1.f, 0.f };

        //unreal engine 
        //float3  origin_;
        //quat  rotate_;

        float   near_clip_{ 1.f };
        float   far_clip_{ 5000.f };
        float   fov_{ 60.f };
        //do not use physX camera to calc fov, so no more need follow argument
        //float   focal_length_{ 1.f }; //FOVv = 2¡Áatan(SensorHeight/2 / focal_lenghth)
        //float   aperture_size_{ 0.f }; //Diameter = FocalLength / Fstop
        float   aspect_ratio_{ 16.f / 9.f };
        uint4   view_port_;

        FORCE_INLINE bool IsPerspective()const {
            return Utils::HasAnyFlags(type_, EType::ePerspective);
        }
        FORCE_INLINE bool IsReverseZ()const {
            return Utils::HasAnyFlags(type_, EType::eReverseZ);
        }

        //update yaw/pitch/roll and translation
        void Update(float3 rotation, float3 tanslation);
    };

    inline float4x4 CalcCameraViewMatrix(const float3& eye, const float3& center, const float3& up)
    {
        return glm::lookAtRH(eye, center, up);
    }

    //view matrix and project matrix calculation
    inline float4x4 CalcCameraViewMatrix(const float3& origin, const quat& rotate)
    {
        //normalize rotate
        auto rot = glm::normalize(rotate);
        float4x4 rot_mat = glm::mat4_cast(rot); //transform rotate quaterin to matrix
        auto inv_rot_mat = glm::transpose(rot_mat);

        //move the world by -rotated(origin)
        auto rot_orign = rot * origin;
        auto inv_translation = glm::translate(glm::mat4(1.f), -rot_orign); //mat4(1) equal to diag(1,1,1,1)

        return inv_translation * inv_rot_mat;
    }

    //calculate perspective projection matrix for right-hand coordinate system
    inline float4x4 CalcCameraPerspectiveProjMatrix(const float fov_y_degrees, const float aspect_ratio, const float near_clip, const float far_clip, const bool reverse_z)
    {
        auto proj = glm::perspectiveFovRH_ZO(glm::radians(fov_y_degrees), aspect_ratio, 1.0f, near_clip, far_clip <= near_clip ? 0.0f : far_clip); //auto infinite
        if (!reverse_z)
        {
            // Convert to NO (flip Z direction + range)
            proj[2][2] = -proj[2][2];   // flip Z scale
            proj[3][2] = -proj[3][2];   // flip Z translate
        }
        return proj;
    }

    //calculate orthographic projection matrix for right-hand coordinate system
    inline float4x4 CalcCameraOrthographicProjMatrix(const uint4 view_port, const float near_clip, const float far_clip, const bool reverse_z)
    {
        // view_port: .x = left (offset), .y = bottom (offset), .z = width, .w = height
        // In ortho, we usually want pixel-perfect or pixel-aligned rendering, so we use integer extents
        // but GLM ortho expects float anyway.

        float left = static_cast<float>(view_port.x);
        float right = left + static_cast<float>(view_port.z);
        float bottom = static_cast<float>(view_port.y);
        float top = bottom + static_cast<float>(view_port.w);

        // Standard ortho: maps to NDC [-1..1] XY, [0..1] or [1..0] Z
        if (reverse_z)
        {
            // Reverse-Z: near=1.0, far=0.0 in NDC ¡ú better floating-point precision
            // GLM ortho handles far < near correctly
            return glm::orthoRH(left, right, bottom, top, far_clip, near_clip);
        }
        else
        {
            return glm::orthoRH(left, right, bottom, top, near_clip, far_clip);
        }
    }

    struct ECSPlayerControllerComponent
    {
        float3  velocity_{ 0.f, 0.f, 0.f };
        float3  angular_velocity_{ 0.f, 0.f, 0.f };
        float   move_speed_{ 5.f };
        float   look_speed_{ 0.1f };
    };

}
