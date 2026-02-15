#pragma once
#include "Utils/CommonUtils.h"

namespace Shard::System::PhyX::VFX
{

    enum class EEmitterType : uint8_t
    {
        eGPUPointEmitter,
        eCPUPointEmitter,
    };

    enum class EParticleType : uint8_t
    {
        eSprite, //sprite/billboard particle
        eMesh,
        eRibbon,
    };

    struct ECSVFXSystem
    {
        //todo
        //a handle to system instance
        //emiiterters
        //todo shaders
        uint32_t    is_sdf_collision_enabled_ : 1;
        uint32_t    is_depth_collision_enabled_ : 1;
    };
}
