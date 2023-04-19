#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include "Utils/FileArchive.h"
#include "Utils/ReflectionImageObject.h"
#include "RHI/RHIGlobalEntity.h"
#include "Renderer/RtRenderShaderUtils.inl"
#include "Renderer/RtRenderShaderParameters.h"
#include "eastl/shared_ptr.h"

namespace MetaInit::Renderer
{
	struct RtRendererShaderParamBinding
	{
		struct BindEntity
		{
			enum class EType : uint8_t
			{
				eScalarOrVec,
				eTexture,
				eBuffer,
				eSampler,
			};
			EType		type_;
			uint32_t	base_index_{ 0u };
			uint32_t	buffer_index_{ 0u };
			uint32_t	byte_offset_{ 0u };
		};
		//binding entities
		SmallVector<BindEntity>	entities_;
	};

	static constexpr uint32_t MAX_PARAM_BINDINGS = 8;
	using RtRendererShaderParamBindings = SmallVector<RtRendererShaderParamBinding, MAX_PARAM_BINDINGS>;

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
		RHI::ERHIBackEnd	back_end_;
		EShaderModel	shader_model_;
		static const String& ToString(const ShaderPlatform& platform);
	};

	//default platform use vulkan sm5
	static constexpr ShaderPlatform g_default_platform{ RHI::ERHIBackEnd::eVulkan, EShaderModel::eSM_5_1 };

	struct RtRenderShaderCode {
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
		RtRenderShaderCode& Compress();
		RtRenderShaderCode& DeCompress();
		uint32_t	magic_num_{ 0u };
		EShaderFrequency	freq_{ EShaderFrequency::eNum };
		HashType	hash_;
		int64_t	time_tag_{ 0 }; //todo fix time rep
		Vector<uint8_t>	code_binary_;
	};

	/*data to initialize shader infos*/
	struct RtRenderShaderInitializeInput
	{
		RtShaderType::HashType	shader_type_;
		RtRenderShaderCode	shader_code_;
		RtShaderParameterInfosMap	shader_params_;
	};

	class MINIT_API RtRenderShader
	{
	public:
		BEGIN_DECLARE_TYPE_LAYOUT_DEF(RtRenderShader);
		using Ptr = RtRenderShader*;
		using SharedPtr = eastl::shared_ptr<RtRenderShader>;
		using PermutationVec = PermutationTupleNULL;
		using HashType = Utils::Blake3Hash64;

		static Ptr Create(const RtRenderShaderInitializeInput& input) {
			return nullptr;
		}

		static Ptr CreateFromArchive() {
			return nullptr;
		}

		static bool ShouldCompile(uint32_t permutation_id) {
			return false;
		}

		struct ShaderParam
		{
			virtual ~ShaderParam() {}
		};
		RtRenderShader() = default;
		//initial render from compiled code
		RtRenderShader(const RtRenderShaderInitializeInput& initializer) {};
		virtual ~RtRenderShader() {}
		const HashType& GetShaderHash()const;
		const uint32_t GetResourceIndex()const;
		const RtShaderType::Ptr GetShaderType()const;
		virtual void Compile(const RtRenderShaderCode& shader_code) = 0;
		virtual bool IsCompileNeedFor(const ShaderPlatform& platform, const uint32_t permutation) = 0;
	protected:
		void BindShaderParameters(const RtShaderParameterInfosMap& param_infos);
		void ComputeHash();
	public:
		LAYOUT_FIELD(RtRendererShaderParamBindings, bindings_);
	protected:
		//indentify shader type by hash
		LAYOUT_FIELD(RtShaderType::HashType, shader_type_);
		LAYOUT_FIELD(HashType, shader_hash_);
		LAYOUT_FIELD(uint32_t, resource_index_);
		LAYOUT_FIELD(uint32_t, permutation_id_);
		END_DECLARE_TYPE_LAYOUT_DEF(RtRenderShader);
	};

	struct RtRenderShaderParametersMeta
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
			EType	type_;
			String	name_;
			uint16_t	byte_offset_{ 0u };
			union {
				uint16_t	size_{ 0u };
				RtRenderShaderParametersMeta* sub_meta_{ nullptr };
			};
		};
		const Element& operator[](uint32_t index)const;
		FORCE_INLINE uint32_t ParameterCount()const {
			return members_.size();
		}
		void AddElement(Element&& element);
		SmallVector<Element>	members_;
	};

	//bind shader parameter by reflection and meta info
	class MINIT_API RtRenderShaderParameterBindHelper
	{
	public:
		explicit RtRenderShaderParameterBindHelper(RtRendererShaderParamBindings& bindings) :bindings_(bindings) {}
		void Bind(const RtRenderShaderParametersMeta& param_meta, const RtShaderParameterInfosMap& param_map);
	private:
		void Bind(const Span<RtRenderShaderParametersMeta::Element>& param_meta, const RtShaderParameterInfosMap& param_map, const String& prefix_name, uint32_t prefix_offset = 0u);
	private:
		RtRendererShaderParamBindings& bindings_;
	};
}