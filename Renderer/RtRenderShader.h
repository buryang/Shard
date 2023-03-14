#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include "Utils/FileArchive.h"
#include "Utils/ReflectionImageObject.h"
#include "Renderer/RtRenderShaderUtils.inl"
#include "eastl/shared_ptr.h"

namespace MetaInit::Renderer
{
	struct RtRendererShaderParamBinding
	{
		enum class Type : uint8_t
		{
			eScalarOrVec,
			eResource,
			eStructInclude,
		};
		Type		type_;
		uint32_t	index_{ 0 };
		uint32_t	byte_offset_{ 0 };
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

	struct RtRenderShaderCode {
		FORCE_INLINE void Serialize(Utils::FileArchive& archive){
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
		RtRenderShaderCode& Compress();
		RtRenderShaderCode& DeCompress();
		using HashType = Utils::Blake3Hash64;
		uint32_t	magic_num_{ 0u };
		EShaderFrequency	freq_{ EShaderFrequency::eNum };
		HashType	hash_;
		int64_t time_tag_{ 0 }; //todo fix time rep
		Vector<uint8_t>	code_binary_;
	};

	class MINIT_API RtRenderShader
	{
	public:
		BEGIN_DECLARE_TYPE_LAYOUT_DEF(RtRenderShader);
		using Ptr = RtRenderShader*;
		using SharedPtr = eastl::shared_ptr<RtRenderShader>;
		using PermutationVec = PermutationTupleNULL;
		using HashType = Utils::Blake3Hash64;

		struct ShaderParam
		{
			virtual ~ShaderParam() {}
		};
		virtual ~RtRenderShader() {}
		static const RtRendererShaderParamBindings& GetShaderBindings() {
			return bindings_;
		}
		/*our hash to identify a shader*/
		static HashType MakeShaderHash(RtRenderShader::Ptr shader, uint32_t permutation_id);
		virtual HashType GetShaderHash() const = 0;
		virtual uint32_t GetShaderPermutationCount() const { return 1u; }
		virtual EShaderFrequency GetShaderFrequency() const = 0;
		virtual void Compile(const RtRenderShaderCode& shader_code) = 0;
		virtual bool IsCompileNeedFor(const uint32_t permutation) = 0;
	protected:
		bool IsShaderSourceFileChanged(const String& meta_path) const;
		FORCE_INLINE void RefreshCompileTime() { time(&compile_time_); }
	protected:
		static RtRendererShaderParamBindings bindings_;
		LAYOUT_FIELD(uint32_t, permutation_id_);
		LAYOUT_FIELD_DEFAULT(std::time_t, compile_time_, 0u); //shader compile time
		END_DECLARE_TYPE_LAYOUT_DEF(RtRenderShader);
	};

	template<class Shader>
	void SetShaderParameters(RHICommandList& cmd, Shader::Ptr shader, const Shader::ShaderParam& parameters) {

	}

	class MINIT_API RtRenderShaderParametersMeta
	{
	public:
		struct Element
		{
			uint16_t	buffer_index_{ 0 };
			uint16_t	base_index_{ 0 };
			uint16_t	byte_offset_{ 0 };
			//type
		};
		const Element& operator[](uint32_t index)const;
		FORCE_INLINE uint32_t ParameterCount()const {
			return members_.size();
		}
		void AddElement(Element&& element);
	private:
		SmallVector<Element>	members_;
	};
}