#include "Renderer/RtRenderShader.h"
#include "Renderer/RtRenderShaderFactory.h"
#include "RHI/RHIShader.h"
#include "RHI/RHIGlobalEntity.h"
#include <folly/Hash.h>
#include <zlib.h>
#include <filesystem>

namespace Shard::Renderer
{
    //layout define implement here
    IMPLEMENT_TYPE_LAYOUT_DEF(RtRenderShader);
    IMPLEMENT_TYPE_LAYOUT_DEF(RtRendererShaderParamBindings);
    IMPLEMENT_TYPE_LAYOUT_DEF(RtRendererShaderParamBindings::BindEntity);

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

    RtRenderShader::RtRenderShader(const RtRenderShaderInitializeInput& initializer):shader_type_(initializer,shader_type_)
    {
        BindShaderParameters(initializer.shader_params_);
    }

    const RtRenderShader::HashType& RtRenderShader::GetShaderHash() const
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


    void RtRenderShader::BindShaderParameters(const RtShaderParameterInfosMap& param_infos)
    {
        RtRenderShaderParameterBindHelper bind_helper(bindings_);
        const auto* param_meta = GetShaderParametersMeta(); //todo 
        if (nullptr != param_meta) {
            bind_helper.Bind(*param_meta, param_infos);
        }
    }

    void RtRenderShader::ComputeHash()const
    {
        HashType hash;
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);
        const auto* shader_type = GetShaderType();
        const auto& type_hash = shader_type->GetShaderHash();
        blake3_hasher_update(&hasher, type_hash.GetBytes(), type_hash.GetHashSize());
        assert(permutation_id < shader_type->GetShaderPermutationCount());
        blake3_hasher_update(&hasher, &permutation_id_, sizeof(permutation_id_));
        blake3_hasher_finalize(&hasher, shader_hash_.GetBytes(), shader_hash_.GetHashSize());
    }

    RtRenderShaderCode& RtRenderShaderCode::Compress()
    {
        if (!IsCompressed()){
            const auto budget_size = uint32_t(code_binary_.size() * 1.01 + 12);
            Vector<uint8_t> compress_buf(budget_size);
            uLongf compressed_size{ 0u };
            auto ret = compress(compress_buf.data(), &compressed_size, code_binary_.data(), code_binary_.size());
            PCHECK(Z_OK == ret) << "failed to compress shader code";
            compress_buf.resize(compressed_size);
            std::swap(compress_buf, code_binary_);
            //change magic num to compress todo 
        }
        return *this;
    }

    RtRenderShaderCode& RtRenderShaderCode::DeCompress()
    {
        if (IsCompressed()) {
            const auto budget_size = uint32_t(code_binary_.size() * 1.01 + 12);
            Vector<uint8_t> decompress_buf(budget_size);
            uLongf decompressed_size{ 0u };
            auto ret = uncompress(decompress_buf.data(), &decompressed_size, code_binary_.data(), code_binary_.size());
            PCHECK(Z_OK == ret) << "failed to uncompress shader code";
            decompress_buf.resize(decompressed_size);
            std::swap(decompress_buf, code_binary_);
            //change magic num to uncompress todo
        }
        return *this;
    }

    void RtRenderShaderParameterBindHelper::Bind(const RtRenderShaderParametersMeta& param_meta, const RtShaderParameterInfosMap& param_map)
    {
        BindImpl(param_meta.members_, param_map);
    }

    void RtRenderShaderParameterBindHelper::BindImpl(const Span<RtRenderShaderParametersMeta::Element>& param_meta, const RtShaderParameterInfosMap& param_map, const String& prefix_name, uint32_t prefix_offset)
    {
        for (const auto& param_info : param_meta) {
            const auto param_byte_offset = param_info.byte_offset_;
            const auto& param_name = param_info.name_;
            if (param_info.type_ == RtRenderShaderParametersMeta::Element::EType::eStructIncluded) {
                BindImpl(param_info.sub_meta_, param_map, param_name, param_byte_offset);
            }
            //to do other type
            RtRendererShaderParamBinding::Entity bind_entity{ .byte_offset_ = param_byte_offset };
            bindings_.emplace_back(bind_entity);
        }
    }

    void RtRenderShaderPipeline::AddShader(RtRenderShader::Ptr shader)
    {
        const auto* shader_type = shader->GetShaderType();
        auto freq = shader_type->GetFrequency();
        shaders_[Utils::EnumToInteger(freq)] = shader;
    }

    void RtRenderShaderPipeline::SetRHI()
    {
        const auto trans_pso2initializer_func = [this](const PipelineStateObjectDesc& desc) {
            RHI::RHIPipelineStateObjectInitializer initializer;
            initializer.reserved_flags_ = desc.user_flags_;
            if (desc.type_ == PipelineStateObjectDesc::EPSOType::eGFX) {
                //initializer.gfx_.
            }
            else if (desc.type_ == PipelineStateObjectDesc::EPSOType::eCompute) {

            }
            else if (desc.type_ == PipelineStateObjectDesc::EPSOType::eRayTracing) {

            }
            else {
                LOG(ERROR) << "pso desc type is not supported";
            }
            //todo render shader and rhi shader relation
            return initializer;
        };
        if (rhi_entity_ == nullptr) {
            const auto& rhi_initializer = trans_pso2initializer_func(desc_);
            RHI::RHIGlobalEntity::Instance()->GetOrCreatePSOLibrary()->GetOrCreatePipeline(rhi_initializer);
        }
    }

    RtRenderShaderPipeline::RtRenderShaderPipeline(const PipelineStateObjectDesc& desc)
    {
        Init(desc);
    }

   
    void RtRenderShaderPipeline::Init(const PipelineStateObjectDesc& desc)
    {
        desc_ = desc;
        use_counter_ = 0;
        last_use_time_ = 0; 

        //add shader
        RtShaderCompiledRepo::Instance().GetShader(desc.gfx_desc_.stage_shaders[]);
        AddShader();


        //set rhi
        SetRHI();
    }

    RtRenderShader::Ptr RtRenderShaderPipeline::FindShader(EShaderFrequency freq)
    {
        return shaders_[Utils::EnumToInteger(freq)];
    }

    bool RtRenderShaderPipeline::IsValid() const
    {
        //todo
        for (auto n = 0u; n < Utils::EnumToInteger(EShaderFrequency::eNum); ++n) {
            auto shader = shaders_[n];
            if (shader != nullptr) {
                const auto freq = shader->GetShaderType()->GetFrequency();
                if (n < Utils::EnumToInteger(EShaderFrequency::eGFXNum)) {
                    if (shader->GetShaderHash() != desc_.gfx_desc_.stage_shaders[freq]) {
                        return false;
                    }

                }
                else if (freq == EShaderFrequency::eCompute) {
                    if (shader->GetShaderHash() != desc_.compute_desc_.compute_shader_) {
                        return false;
                    }
                }
                else 
                {
                    //to do ray trace pipeline 
                    return false;
                }
            }
        }
        return true;
    }
}
