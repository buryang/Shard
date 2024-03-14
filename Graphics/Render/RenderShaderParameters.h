#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Reflection.h"
#include "Render/RenderShaderUtils.inl"

namespace Shard::Render {

    struct ShaderParametersReflection
    {
        String name_;
        uint32_t base_index_{ 0u };
        uint32_t buffer_index_{ 0u };
    };

    using RenderShaderParameterInfosMap = Map<const String&, ShaderParametersReflection>;

    //parameters for shader def
    enum class EShaderResourceType {
        eTrivalData,
        eBuffer,
        eTexture,
        eSampler,
        eAS,
        eNum,
    };

    struct ShaderResourceParameter
    {
        String    name_;
        EShaderResourceType    type_;
        uint32_t    base_index_{ 0u };
        uint32_t    buffer_index_{ 0u };
        uint32_t    buffer_size_{ 0u };
    };

    class RenderShaderParameterInfosMapUtils
    {
    public:
        using HashType = Utils::Blake3Hash64;
        explicit RenderShaderParameterInfosMapUtils(RenderShaderParameterInfosMap& map);
        bool FindShaderParameters(const String& name, ShaderParametersReflection& allocation)const;
        void AddShaderParameters(const ShaderParametersReflection& allocation);
        void RemoveShaderParameters(const String& name);
        const HashType& ComputeHash()const;
    private:
        RenderShaderParameterInfosMap& map_;
    };

    void GetSPIRVShaderParameterInfosReflection(const Span<uint8_t>& spriv_code, RenderShaderParameterInfosMap& map);
}