#pragma once
#include "Utils/Hash.h"
#include "Utils/FileArchive.h"
#include "Core/PixelInfo.h"
#include "Scene/Primitive.h"
#include "HAL/HALGlobalEntity.h"
#include "HAL/HALShaderLibrary.h"
#include "Render/RenderShader.h"
#include <shared_mutex>

namespace Shard::Render {
    using Utils::FileArchive;
    struct PipelineStateObjectDesc {
        using HashVal = RenderShader::HashType;
        //union {
            struct ComputEPipelineDesc {
                HashVal    compute_shader_;
            } compute_desc_;
            struct GraphicsPipelineDesc {
                HashVal stage_shaders[Utils::EnumToInteger(EShaderFrequency::eGFXNum)];
                HAL::HALVertexInputStateInitializer    vertex_input_state_;
                HAL::HALBlendStateInitializer    blend_state_;
                HAL::HALDepthStencilStateInitializer    depth_stencil_state_;
                HAL::HALRasterizationStateInitializer    rasterization_state_;
                //assembly state only has a topology flags
                EInputTopoType    primitive_topology_{ EInputTopoType::eUnkown };
            } gfx_desc_;
            struct RayTracEPipelineDesc {
                HashVal    miss_hit_;
                HashVal    any_hit_;
                HashVal    closest_hit_;
                HashVal    raygen_;
                HashVal    callable_;
            } raytracing_desc_;
        //};
        enum class EPSOType {
            eUnkown,
            eCompute,
            eGFX,
            eRayTracing,
        };
        EPSOType    type_{ EPSOType::eUnkown };
        uint32_t    user_flags_{ 0u };
        static HashVal ComputeHash(const PipelineStateObjectDesc& desc);
    };

    struct PSOArchiveHeader
    {
        //todo whether need a header 
    };

    FileArchive& operator<<(FileArchive& ar, PipelineStateObjectDesc& pso);

    enum class EShaderCompressMode : uint8_t
    {
        eNone = 0x0,
        eCompressed = 0x1,
    };

    struct ShaderSectionDesc {
        uint32_t    offset_{ 0u };
        uint32_t    size_{ 0u };
        EShaderCompressMode    compress_{ EShaderCompressMode::eNone };
        Utils::Blake3Hash64    hash_;
    };
    
    enum class EIRBackend {
        eUnkown,
        eDXIL,
        eSPIRV,
    };

    struct ShaderArchiveHeader {
        static constexpr const uint32_t    SHADER_HEADER_SIZE = 1000u;
        static constexpr const uint32_t SHADER_HEADER_MAGIC_NUM = ;
        uint32_t    magic_{ SHADER_HEADER_MAGIC_NUM };
        EIRBackend    ir_{ EIRBackend::eSPIRV };
        EShaderModel    shader_model_;
        //platfom
        SmallVector<ShaderSectionDesc>    sections_;
        FORCE_INLINE bool IsHeaderFull() const {
            return sizeof(magic_) + sizeof(sections_.size() * sizeof(ShaderSectionDesc)) >= SHADER_HEADER_SIZE;
        }
    };

    FileArchive& operator<<(FileArchive& ar, ShaderArchiveHeader& archive_header);

    class RenderShaderArchive {
    public:
        using Ptr = RenderShaderArchive*;
        using HashType = RenderShaderCode::HashType;
        void Serialize(FileArchive& ar, bool use_compress=false);
        void Unserialize(FileArchive& ar);
        /*how to deal with shader code update*/
        void AddShaderCode(const RenderShaderCode& shader_code);
        void RemoveShaderCode(const HashType& hash);
        const RenderShaderCode& GetShaderCode(uint32_t shader_index) const;
        const RenderShaderCode& GetShaderCode(const HashType& hash) const;
        const uint64_t GetArchiveSize() const;
        const uint32_t GetShadersCount() const;
        void Clear();
        bool IsEmpty() const;
    private:
        EIRBackend    ir_{ EIRBackend::eSPIRV };
        EShaderModel    shader_model_;
        Vector<HashType>    shader_hashes_;
        Vector<RenderShaderCode>    shader_codes_;
        mutable std::shared_mutex    shader_rw_mutex_;
    };

}
