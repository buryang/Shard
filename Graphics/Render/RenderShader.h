#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include "Utils/FileArchive.h"
#include "Utils/ReflectionImageObject.h"
#include "RHI/RHIGlobalEntity.h"
#include "RHI/RHIShader.h"
#include "Render/RenderShaderUtils.inl"
#include "Render/RenderShaderParameters.h"
#include "Render/RenderShader.h"
#include "Render/RenderShaderFactory.h"
#include "eastl/shared_ptr.h"
#include <atomic>

namespace Shard::Render
{
    struct RenderShaderParamBindings
    {
        struct BindEntity
        {
            BEGIN_DECLARE_TYPE_LAYOUT_DEF(BindEntity);
            enum class EType : uint8_t
            {
                eScalarOrVec,
                eTexture,
                eBuffer,
                eSampler,
            };
            LAYOUT_FIELD_DEFAULT(EType, type_);
            LAYOUT_FIELD_DEFAULT(,uint32_t,base_index_,0u);
            LAYOUT_FIELD_DEFAULT(,uint32_t,buffer_index_,0u);
            LAYOUT_FIELD_DEFAULT(,uint32_t,byte_offset_,0u);
            END_DECLARE_TYPE_LAYOUT_DEF(BindEntity);
        };
        //binding entities
        enum { MAX_PARAM_BINDINGS = 8u };
        BEGIN_DECLARE_TYPE_LAYOUT_DEF(RenderShaderParamBindings);
        LAYOUT_ARRAY_FIELD(, BindEntity, entities_, MAX_PARAM_BINDINGS);
        END_DECLARE_TYPE_LAYOUT_DEF(RenderShaderParamBindings)
    };

    enum class EShaderFrequency :uint8_t
    {
        eVertex,
        eHull,//tessellation control
        eGeometry,
        eDomain,
        eFrag,
        eCompute,
        eRayGen,
        eRayIntersection,
        eRayAnyHit,
        eRayCloseHit,
        eRayMiss,
        eCallAble,
        eNum,
        eGFXNum = eFrag + 1,
    };

    enum class EShaderModel :uint16_t
    {
        eUnkown = 0x00,
        eSM_4_0 = 0x40,
        eSM_5_1 = 0x51,
        eSM_6_0 = 0x60,
        eSM_6_1 = 0x61,
        eSM_6_2 = 0x62,
        eSM_6_3 = 0x63,
        eSM_6_4 = 0x64,
        eSM_6_5 = 0x65,
        eSM_6_6 = 0x66,
        eSM_6_7 = 0x67,
    };

    struct ShaderPlatform
    {
        RHI::ERHIBackEnd    back_end_;
        EShaderModel    shader_model_;
        static const String& ToString(const ShaderPlatform& platform);
    };

    //default platform use vulkan sm5
    static constexpr ShaderPlatform g_default_platform{ RHI::ERHIBackEnd::eVulkan, EShaderModel::eSM_5_1 };

    struct RenderShaderCode {
        using HashType = Utils::Blake3Hash64;
        enum {
            eCompressBitFlag = 1 << 31
        };
        FORCE_INLINE void Serialize(Utils::FileArchive& archive) {
            archive << magic_num_;
            archive << freq_;
            archive << hash_;
            archive << time_tag_;
            archive << code_binary_.size();
            const auto& check_sum = Utils::CalcBlake3HashForBytes<Utils::Blake3Hash256::MAX_HASH_SIZE>(code_binary_.data(), code_binary_.size());
            archive << check_sum;
            archive.Serialize(reinterpret_cast<void*>(code_binary_.data()), code_binary_.size());
        }
        FORCE_INLINE void UnSerialize(Utils::FileArchive& archive) {
            archive << magic_num_;
            archive << freq_;
            archive << hash_;
            archive << time_tag_;
            std::size_t bin_size{ 0 };
            archive << bin_size;
            code_binary_.resize(bin_size);
            Utils::Blake3Hash256 check_sum;
            archive << check_sum;
            archive.Serialize(reinterpret_cast<void*>(code_binary_.data()), bin_size);
            const auto& native_sum = Utils::CalcBlake3HashForBytes<Utils::Blake3Hash256::MAX_HASH_SIZE>(code_binary_.data(), code_binary_.size());
            if (check_sum != native_sum) {
                LOG(ERROR) << "unserialize shader binary failed";
            }
        }
        FORCE_INLINE bool IsCompressed()const {
            return magic_num_ & eCompressBitFlag;
        }
        FORCE_INLINE void SetHash(const HashType& hash) {
            hash_ = hash;
        }
        RenderShaderCode& Compress();
        RenderShaderCode& DeCompress();
        uint32_t    magic_num_{ 0u };
        EShaderFrequency    freq_{ EShaderFrequency::eNum };
        HashType    hash_;
        int64_t    time_tag_{ 0 }; //todo fix time rep
        Vector<uint8_t>    code_binary_;
    };

    struct RenderShaderParametersMeta
    {
        struct Element
        {
            enum class EType
            {
                eScalarOrVec,
                eResource,
                eReference,
                eStructIncluded, //include another parameters
            };
            EType    type_;
            String    name_;
            uint16_t    byte_offset_{ 0u };
            uint16_t    size_{ 0u };
            RenderShaderParametersMeta* sub_meta_{ nullptr };
        };
        const Element& operator[](uint32_t index)const;
        FORCE_INLINE uint32_t ParameterCount()const {
            return members_.size();
        }
        void AddElement(Element&& element);
        SmallVector<Element>    members_;
    };

    //bind shader parameter by reflection and meta info
    class MINIT_API RenderShaderParameterBindHelper
    {
    public:
        explicit RenderShaderParameterBindHelper(RenderShaderParamBindings& bindings) :bindings_(bindings) {}
        void Bind(const RenderShaderParametersMeta& param_meta, const RenderShaderParameterInfosMap& param_map);
    private:
        void BindImpl(const Span<RenderShaderParametersMeta::Element>& param_meta, const RenderShaderParameterInfosMap& param_map, const String& prefix_name=String(""), uint32_t prefix_offset = 0u);
    private:
        RenderShaderParamBindings& bindings_;
    };

    /*data to initialize shader infos*/
    struct RenderShaderInitializeInput
    {
        RenderShaderType::HashType    shader_type_;
        RenderShaderCode    shader_code_;
        RenderShaderParameterInfosMap    shader_params_;
    };

    class ShaderCompileEnv;
    class MINIT_API RenderShader
    {
        BEGIN_DECLARE_TYPE_LAYOUT_DEF(RenderShader);
    public:
        using Ptr = RenderShader*;
        using SharedPtr = eastl::shared_ptr<RenderShader>;
        using PermutationVec = PermutationTupleNULL;
        using HashType = Utils::Blake3Hash64;

        static Ptr Create(const RenderShaderInitializeInput& input) {
            return nullptr;
        }

        static Ptr CreateFromArchive() {
            return nullptr;
        }

        static bool ShouldCompile(const ShaderCompileEnv& env, uint32_t permutation_id) {
            return false;
        }

        struct ShaderParam
        {
            virtual ~ShaderParam() {}
        };
        RenderShader() = default;
        //initial render from compiled code
        RenderShader(const RenderShaderInitializeInput& initializer);
        virtual ~RenderShader() {}
        const HashType& GetShaderHash()const;
        const uint32_t GetResourceIndex()const;
        const RenderShaderType::Ptr GetShaderType()const;
        virtual bool IsCompileNeedFor(const ShaderPlatform& platform, const uint32_t permutation) = 0;
        virtual RenderShaderParametersMeta* GetShaderParametersMeta() = 0;
    protected:
        void BindShaderParameters(const RenderShaderParameterInfosMap& param_infos);
        void ComputeHash()const;
    public:
        LAYOUT_FIELD(,RenderShaderParamBindings, bindings_);
    protected:
        //indentify shader type by hash
        LAYOUT_FIELD(,RenderShaderType::HashType, shader_type_);
        LAYOUT_FIELD(mutable,HashType, shader_hash_);
        LAYOUT_FIELD_DEFAULT(,uint32_t, permutation_id_, 0u);
        END_DECLARE_TYPE_LAYOUT_DEF(RenderShader);
    };

    class MINIT_API RenderShaderPipeline
    {
    public:
        using SharedPtr = eastl::shared_ptr<RenderShaderPipeline>;
        RenderShaderPipeline() = default;
        explicit RenderShaderPipeline(const PipelineStateObjectDesc& desc);
        void Init(const PipelineStateObjectDesc& desc);
        FORCE_INLINE const PipelineStateObjectDesc& GetPipelineDesc() const {
            return desc_;
        }
        FORCE_INLINE const PipelineStateObjectDesc::EPSOType GetPipelineTye() const {
            return desc_.type_;
        }
        FORCE_INLINE const PipelineStateObjectDesc::HashVal GetHash()const {
            return PipelineStateObjectDesc::ComputeHash(desc_);
        }
        /*
        FORCE_INLINE RenderShader::Ptr operator[](EShaderFrequency freq) {
            return shaders_[Utils::EnumToInteger(freq)];
        }
        */
    private:
        PipelineStateObjectDesc    desc_;
        RHI::RHIPipelineStateObject::Ptr    rhi_entity_{ nullptr };
        //fixme not need this?
        //RenderShader::Ptr shaders_[Utils::EnumToInteger(EShaderFrequency::eNum)];
        //to do whehter define here
        mutable std::atomic_uint32_t    use_counter_{ 0u };
        mutable std::atomic_uint32_t    last_use_time_{ 0u };
    };
}