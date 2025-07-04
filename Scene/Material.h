#pragma once
#include "Utils/CommonUtils.h"
#include "Graphics/HAL/HALShader.h"

namespace Shard::Scene
{

    template<typename...T>
    struct TMaterialShaderTuple
    {
        Tuple<T...> shaders_;
        template<typename ShaderType>
        HAL::HALShader* GetShader() const {
            shaders_.get<ShaderType>();
        }
    };

    using MaterialShaders = TMaterialShaderTuple<HAL::HALVertexShader*, HAL::HALHullShader*, HAL::HALPixelShader*>; //todo

    class MINIT_API PrimitiveMaterialInterface
    {
    public:
        using HashName = String; //todo
        const auto& GetShaders() const { return material_shaders_; }
        virtual ~PrimitiveMaterialInterface() = 0;
    protected:
        MaterialShaders material_shaders_;
    };

    class MINIT_API PrimitiveMaterial : public PrimitiveMaterialInterface
    {
    public:
        virtual ~PrimitiveMaterial() = default;
        bool HasParent() const { return parent_ != nullptr; }
        bool IsVariant() const { return is_variant_; }
        bool IsDoubleSided() const { return is_double_sided_; }
        bool IsInstancingEnabled() const { return is_instancing_enabled_; }
    protected:
        PrimitiveMaterialInterface* parent_{ nullptr };
        //material statistics
        uint16_t    pass_count_{ 0u }; //pass count 
        uint16_t    render_queue_{ 0u }; //material render queue index

        uint32_t    is_variant_ : 1;
        uint32_t    is_double_sided_ : 1;
        uint32_t    is_instancing_enabled_ : 1;

        //global illumination emissive flags
        uint32_t    gi_realtime_emissive_ : 1;
        uint32_t    gi_bake_emissive_ : 1;
        uint32_t    gi_black_emissive_ : 1;
    };
    
    //material component
    struct ECSMaterialComponent : ECSyncObjectRefComponent<PrimitiveMaterial>
    {
    };
}
