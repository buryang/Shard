#pragma once
#include "Utils/CommonUtils.h"
#include <type_traits>

//shader parameter define macros
#define BEGIN_SHADER_PARAMETER_STRUCT(T, Name, PREFIX, ...) \
PREFIX class T {											\	
private: static constexpr uint32_t counter_base_ = __COUNTER__; \
protected: \
	template <uint32_t index> class InternalLinkType {
	\
};



#define END_SHADER_PARAMETER_STRUCT() \
private: static RtRenderShaderParametersMeta meta_info_; \
};

	   //generate parameter struct glsl file
#define IMPLEMENT_SHADER_PARAMETER_STRUCT(ParamStruct, GLSL_INCLUDE) \


#define INTERNAL_INIT_SHADER_PARAMETER_LAYOUT(MemberName, Type, ...)\
protected: \
template <> class InternalLinkType<__COUNTER__-counter_base> { \
}; 

#define SHARER_PARAMETER(T, Member) \
private: T Member;					\
INTERNAL_INIT_SHADER_PARAMETER_LAYOUT()

#define SHADER_TEXTURE(T, Member) \
private: T member;				  \
INTERNAL_INIT_SHADER_PARAMETER_LAYOUT()

#define SHADER_BUFFER(T, Member) \
private: T member;				\
INTERNAL_INIT_SHADER_PARAMETER_LAYOUT()

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

}