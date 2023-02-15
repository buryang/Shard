#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/FileArchive.h"
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

	enum class EShaderFrequency :uint16_t
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
			archive << code_binary_.size();
			const std::size_t check_sum = Hash<>(code_binary_);
			archive << check_sum;
			archive.Serialize(reinterpret_cast<void*>(code_binary_.data()), code_binary_.size());
		}
		FORCE_INLINE void UnSerialize(Utils::FileArchive& archive) {
			archive << magic_num_;
			std::size_t bin_size{ 0 };
			archive << bin_size;
			code_binary_.resize(bin_size);
			std::size_t check_sum{ 0 };
			archive << check_sum;
			archive.Serialize(reinterpret_cast<void*>(code_binary_.data()), bin_size);
			if (check_sum != Hash<>(code_binary_)) {
				LOG(ERROR) << "unserialize shader binary failed";
			}
		}
		std::size_t	magic_num_{ 0 };
		Vector<uint8_t>	code_binary_;
	};

	class MINIT_API RtRenderShader
	{
	public:
		using Ptr = RtRenderShader*;
		using SharedPtr = eastl::shared_ptr<RtRenderShader>;
		using PermutationVec = PermutationTupleNULL;

		struct ShaderParam
		{
			virtual ~ShaderParam() {}
		};
		virtual ~RtRenderShader() {}
		static const RtRendererShaderParamBindings& GetShaderBindings() {
			return bindings_;
		}
		virtual EShaderFrequency GetShaderFrequency()const=0;
		virtual void Compile(const RtRenderShaderCode& shader_code) = 0;
		virtual bool IsCompileNeedFor(const uint32_t permutation) = 0;
	protected:
		bool IsShaderSourceFileChanged(const String& meta_path) const;
		FORCE_INLINE void RefreshCompileTime() { time(&compile_time_); }
	protected:
		REGIST_SHADER_IMPL();
		static RtRendererShaderParamBindings bindings_;
		uint32_t	permutation_id_;
		std::time_t	compile_time_{ 0 }; //shader compile time
	};

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