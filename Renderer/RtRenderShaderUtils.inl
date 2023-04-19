#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHICommandIR.h"
#include <type_traits>

namespace MetaInit::Renderer {

	struct PermutaionBool {
		using Type = bool;
		static constexpr const uint32_t PermutationCount = 2;
		static constexpr const uint32_t	Dimensions = 1;
		void Set(Type val) {
			dimension_val_ = val;
		}
		Type Get() {
			return dimension_val_;
		}
		Type dimension_val_;
	};

	template <typename IntType, IntType RangeBegin, IntType RangeSize>
	requires std::is_integral_v<IntType>
	struct PermutationInt {
		using Type = IntType;
		static constexpr const uint32_t PermutaionCount = RangeSize;
		static constexpr const uint32_t Dimensions = 1;
		void Set(Type val) {
			if (val < RangeBegin || val > RangeBegin + RangeSize) {
				LOG(ERROR) << "set value is outof range";
			}
			dimension_val_ = val - RangeBegin;
		}
		Type Get() {
			return dimension_val + RangeBegin;
		}
		Type dimension_val_;
		Type range_start_;
	};

	template <typename IntType, IntType ...Values>
	requires std::is_integral_v<IntType>
	struct PermutationSparseInt {
		void Set(Type val) {

		}
		Type Get() {

		}
	};

	template <typename IntType, IntType DimensionVal, IntType ...Values>
	requires std::is_integral_v<IntType>
	struct PermutionSparseInt {
		using Type = IntType;
		void Set(Type val) {

		}
		Type Get() {
			return dimension_val_;
		}
		Type dimension_val_;
	};

	//shader permutation auxiliary function
	template<class ...Permutation>
	class PermutationTuple
	{
	public:
		using Type = PermutationTuple<Permutation...>;
		static constexpr const auto Dimensions = 0;
		static constexpr const auto PermutationCount = 1;
		template <class DimensionToSet>
		void Set(typename DimensionToSet::Type) {
			LOG(INFO) << "NULL Permutation tuple";
		}
		template <class DimensionToGet>
		typename DimensionToGet::Type& Get() const {
			LOG(ERROR) << "NULL Permutation tuple Get NULL Val";
		}
		void FromPermutationID(uint32_t permutation) {
			assert(permutation == 1);
			LOG(INFO) << "NULL Permutaion tuple set permutation";
		}
		uint32_t ToPermutationID()const {
			LOG(INFO) << "NULL permutation tuple to permutation id";
			return 1;
		}
	};

	using PermutationTupleNULL = PermutationTuple<>;

	template<class DimensionType, class ...Permutation>
	class PermutationTuple
	{
	public:
		using Type = PermutationTuple<DimensionType, Permutation...>;
		using Super = PermutationTuple<Permutation...>;
		static constexpr const auto Dimensions = Super::Dimensions + 1;
		static constexpr const auto PermutaionCount = Super::PermutationCount * DimensionType::PermutationCount;
		template <class DimensionToSet>
		void Set(DimensionToSet::Type val) {
			if (std::is_same_v<DimensionType, DimensionToSet>) {
				dimension_val_.Set(val);
			}
			else {
				tail_.Set(val);
			}
		}
		template <class DimensionToGet>
		typename DimensionToGet::Type& Get() const {
			if (std::is_same_v<DimensionType, DimensionToGet>) {
				return dimension_val_.Get();
			}
			else {
				return tail.template Get<DimensionToGet>();
			}
		}
		void FromPermutationID(uint32_t permutation) {
			auto value = permutation % DimensionType::PermutationCount;
			dimension_val.Set(value);
			tail.FromPermutationID(permutation / DimensionType::PermutationCount);
		}
		uint32_t ToPermutationID() const {
			uint32_t permutation = dimension_val_.Get();
			return permutation * tail_.ToPermutationID();
		}

	private:
		Super tail_;
		DimensionType dimension_val_;
	};

	template <typename T, uint32_t alignment>
	struct TypeAlignmentDefine
	{
		using Type = alignas(alignment) T;
	};

	template <typename T>
	struct ShaderParameterTypeTraits
	{
		enum {
			rows_ = 1,
			cols_ = 1,
			element_size_ = 0,
			alignment_ = 4, //align to register 
		};
		using AlignType = TypeAlignmentDefine<T, alignment_>;
	};

	#define VECTOR_SHADER_PARAMETER_TRAITS(TYPE, SIZE)\
	template<>\
	struct ShaderParameterTypeTraits<TYPE##vec##SIZE>\
	{\
		enum {\
			rows_ = 1,\
			cols_ = SIZE,\
			element_size_ = SIZE,\
			alignment_ = SIZE > 2 ? 16 : 8, \
		};\
		using AlignType = TypeAlignmentDefine<TYPE##vec##SIZE, alignment_>;\
	};
	
	VECTOR_SHADER_PARAMETER_TRAITS(, 2);
	VECTOR_SHADER_PARAMETER_TRAITS(, 3);
	VECTOR_SHADER_PARAMETER_TRAITS(i, 2);
	VECTOR_SHADER_PARAMETER_TRAITS(i, 3);
	VECTOR_SHADER_PARAMETER_TRAITS(u, 2);
	VECTOR_SHADER_PARAMETER_TRAITS(u, 3);
	VECTOR_SHADER_PARAMETER_TRAITS(b, 2);
	VECTOR_SHADER_PARAMETER_TRAITS(b, 3);

	#define MATRIX_SHADER_PARAMETER_TRAITS(TYPE, DIMS)\
	template<>\
	struct ShaderParameterTypeTraits<TYPE##mat##DIMS>\
	{\
		enum{\
			rows_ = DIMS,\
			cols_ = DIMS,\
			element_size_ = DIMS*DIMS,\
			alignment_ = 16, \
		};\
		using AlignType = TypeAlignmentDefine<TYPE##mat##DIMS, alignment_>;\
	};

	MATRIX_SHADER_PARAMETER_TRAITS(, 3);
	MATRIX_SHADER_PARAMETER_TRAITS(, 4);

	template<typename T, uint32_t SIZE>
	struct ShaderParameterTypeTraits<T[SIZE]>
	{
		enum {
			POINTER_ALIGNMENT = 8;
		};
		enum {
			rows_ = 1,
			cols_ = 1,
			element_size_ = SIZE,
			alignment_ = POINTER_ALIGNMENT, //??
		};
		using AlignType = //
	};

}


//shader parameter define macros
#define SHADER_PARAMETER_ALIGNMENT 16
#define BEGIN_SHADER_PARAMETER_STRUCT(TName, PREFIX, ...) \
PREFIX alignas(SHADER_PARAMETER_ALIGNMENT) class TName{ \	
private: static constexpr uint32_t counter_base = __COUNTER__; \
protected:\
	template <uint32_t index> class InternalLinkType { \
	public:\
		operator() {\
		\
		}\
	};



#define END_SHADER_PARAMETER_STRUCT() \
private: static struct RtRenderShaderParametersMeta meta_info_; \
public:\
	struct RtRenderShaderParametersMeta& GetParameterMetaInfo()const\
	{\
		return meta_info_;\
	}\
};

#define INTERNAL_INIT_SHADER_PARAMETER_LAYOUT(MemberName, Type, ...)\
protected: \
template <> class InternalLinkType<__COUNTER__-counter_base> { \
	meta_info_.xxx();\
}; 

#define SHARER_PARAMETER(T, Member) \
private: ShaderParameterTypeTraits<T>::AlignType Member;					\
INTERNAL_INIT_SHADER_PARAMETER_LAYOUT()

#define SHADER_PARAMETER_ARRAY(T, Member, Size)\
private: ShaderParameterTypeTraits<T>::AlignType Member[Size];\
INTERNAL_INIT_SHADER_PARAMETER_LAYOUT()

#define SHADER_TEXTURE(T, Member) \
private: ShaderParameterTypeTraits<T>::AlignType Member;				  \
INTERNAL_INIT_SHADER_PARAMETER_LAYOUT()

#define SHADER_BUFFER(T, Member) \
private: ShaderParameterTypeTraits<T>::AlignType Member;				\
INTERNAL_INIT_SHADER_PARAMETER_LAYOUT()

template<class RHICmdList, class ShaderRHI, class ShaderClass>
void SetShaderParameters(RHICmdList& cmd_list, typename ShaderClass::Ptr shader, typename ShaderRHI::Ptr shader_rhi, const typename ShaderClass::ShaderParameter* parameters) {
	using EBindEntityType = RtRendererShaderParamBinding::BindEntity::EType;
	using EResourceEntityType = RHISetReourcePacket::EResourceType;
	const auto* params_stream = reinterpret_cast<const uint8_t*>(parameters);
	const auto bindings = shader->bindings_;
	
	for(const auto& entity : bindings.entities_)
	{
		const auto entity_type = entity.type_;
		RHISetResourcePacket res_cmd;
		res_cmd.base_index_ = 0u; //todo
		if (entity_type == EBindEntityType::eScalarOrVec) {
			res_cmd.resource_type_ = EResourceEntityType::eTrivalData;
			res_cmd.trival_data_ = params_stream + entity.byte_offset_;
		}
		else 
		{
			const auto* resource_ptr = params_stream + entity.byte_offset_;
			if (entity_type == EBindEntityType::eTexture) {
				res_cmd.resource_type_ = EResourceEntityType::eTexture;
				res_cmd.texture_ = reinterpret_cast<RHITexture::Ptr>(resource_ptr);
			}
			else if (entity_type == EBindEntityType::eBuffer) {
				res_cmd.resource_type_ = EResourceEntityType::eBuffer;
				res_cmd.buffer_ = reinterpret_cast<RHIBuffer::Ptr>(resource_ptr);
			}
			else if (entity_type == EBindEntityType::eSampler) {
				res_cmd.resource_type_ = EResourceEntityType::eSampler;
				res_cmd.sampler_ = reinterpret_cast<RHISampler::Ptr>(resource_ptr);
			}
		}
		cmd_list.Enqueue(res_cmd);
	}
}