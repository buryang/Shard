#include "Renderer/RtRenderShader.h"
#include <folly/Hash.h>
#include <zlib.h>
#include <filesystem>

namespace MetaInit::Renderer
{
    const RtRenderShaderParametersMeta::Element& RtRenderShaderParametersMeta::operator[](uint32_t index) const
    {
        assert(index < members_.size() && "index overflow array members");
        return members_[index];
    }

    void RtRenderShaderParametersMeta::AddElement(RtRenderShaderParametersMeta::Element&& element)
    {
        members_.emplace_back(element);
    }

    static inline uint64_t GenerateBinaryHash(const Span<uint8_t>& bin) {
        return folly::hash::commutative_hash_combine_range_generic(bin.size(), folly::Hash{}, bin.begin(), bin.end());
    }

    void RtRenderShaderCode::Serialize(Utils::FileArchive& archive) const
    {
        archive << magic_num_;
        archive << code_binary_.size();
        archive.Serialize(const_cast<uint8_t*>(code_binary_.data()), code_binary_.size());
        auto hash_value = GenerateBinaryHash(code_binary_);
        archive << hash_value;
    }

    void RtRenderShaderCode::UnSerialize(Utils::FileArchive& archive)
    {
        uint32_t size = 0;
        archive << magic_num_;
        archive << size;
        code_binary_.resize(size);
        archive.Serialize(code_binary_.data(), size);
        //verify data check code
        uint64_t hash_value{ 0 };
        archive << hash_value;
        auto hash_gen = GenerateBinaryHash(code_binary_);
        if (hash_gen != hash_value) {
            LOG(FATAL) << "shader code is not correct";
        }
    }
    
    const String& ShaderPlatform::ToString(const ShaderPlatform& platform)
    {
        auto& back_end = String("UnKown");
        if (platform.back_end_ == RHI::ERHIBackEnd::eVu) {
            back_end = "";
        }

        auto& shader_model = String();
        switch (platform.shader_model_) {
        case EShaderModel::eSM_4_0:
            shader_model = "";
        default:
        }
        return std::format("{}{}", back_end, shader_model);
    }

    const HashType& RtRenderShader::GetShaderHash() const
    {
        if (shader_hash_.IsZero()) {
            ComputeHash();
        }
        return shader_hash_;
    }

    const uint32_t RtRenderShader::GetResourceIndex() const
    {
        assert(resource_index_ != -1);
        return resource_index_;
    }

    const RtShaderType::Ptr RtRenderShader::GetShaderType() const
    {
        const auto* shader_type = RtShaderType::GetShaderType(shader_type_);
        PCHECK(shader_type != nullptr) << "shader correspond type not found";
        return shader_type;
    }

    void RtRenderShader::ComputeHash()
    {
        HashType hash;
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);
        auto* shader_type = GetShaderType();
        const auto& type_hash = shader_type->GetShaderHash();
        blake3_hasher_update(&hasher, type_hash.GetBytes(), type_hash.GetHashSize());
        assert(permutation_id < shader_type->GetShaderPermutationCount());
        blake3_hasher_update(&hasher, &permutation_id, sizeof(permution_id));
        blake3_hasher_finalize(&hasher, shader_hash_.GetBytes(), shader_hash_.GetHashSize());
    }

    RtRenderShaderCode& RtRenderShaderCode::Compress()
    {
        if (!IsCompressed())
            const auto budget_size = uint32_t(code_binary_.size() * 1.01 + 12);
            Vector<uint8_t> compress(budget_size);
            uint32_t compressed_size{ 0u };
            auto ret = compress(compress.data(), &compressed_size, code_binary_.data(), code_binary_.size());
            PCHEK(Z_OK == ret) << "failed to compress shader code";
            compress.resize(compressed_size);
            std::swap(compress, code_binary_);
            //change magic num to compress todo 
        }
        return *this;
    }

    RtRenderShaderCode& RtRenderShaderCode::DeCompress()
    {
        if (IsCompressed()) {
            const auto budget_size = uint32_t(code_binary_.size() * 1.01 + 12);
            Vector<uint8_t> uncompress(budget_size);
            uint32_t decompressed_size{ 0u };
            auto ret = uncompress(compress.data(), &decompressed_size, code_binary_.data(), code_binary_.size());
            PCHEK(Z_OK == ret) << "failed to uncompress shader code";
            uncompress.resize(decompressed_size);
            std::swap(uncompress, code_binary_);
            //change magic num to uncompress todo
        }
        return *this;
    }

    void RtRenderShaderParameterBindHelper::Bind(const RtRenderShaderParametersMeta& param_meta, const RtShaderParameterInfosMap& param_map)
    {
        Bind(param_meta.members_, param_map, "", 0u);
    }

    void RtRenderShaderParameterBindHelper::Bind(const Span<RtRenderShaderParametersMeta::Element>& param_meta, const RtShaderParameterInfosMap& param_map, const String& prefix_name, uint32_t prefix_offset)
    {
        for (const auto& param_info : param_meta) {
            const auto param_byte_offset = param_info.byte_offset_;
            const auto& param_name = param_info.name_;
            if (param_info.type_ == RtRenderShaderParametersMeta::Element::EType::eStructIncluded) {
                Bind(param_info.sub_meta_, param_map, param_name, param_byte_offset);
            }
            //to do other type
            RtRendererShaderParamBinding::Entity bind_entity{ .byte_offset_ = param_byte_offset };
            bindings_.emplace_back(bind_entity);
        }
    }
}
