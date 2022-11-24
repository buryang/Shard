#pragma once

#ifdef _MSC_VER
#define MINIT_API __declspec(dllexport)
#define GLOG_NO_ABBREVIATED_SEVERITIES
#else 
#define MINIT_API
#endif 

#define FORCE_INLINE	__forceinline 
#define ALIGN_AS(align)	alignas(align) 

#define GLM_FORCE_CTOR_INIT
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glog/logging.h"
#include "eastl/vector.h"
#include "eastl/fixed_vector.h"
#include "eastl/hash_map.h"
#include "eastl/fixed_hash_map.h"
#include "eastl/hash_set.h"
#include "eastl/list.h"
#include "eastl/queue.h"
#include "eastl/stack.h"
#include "eastl/span.h"
#include "eastl/optional.h"
#include "eastl/string.h"
#include "eastl/bitset.h"
#include "eastl/bitvector.h"
#include "eastl/internal/thread_support.h"
#include <memory>
#include <algorithm>
#include <assert.h>



#define DISALLOW_COPY_AND_ASSIGN(class_name) \
class_name(const class_name##&)=delete; \
class_name##& operator=(const class_name##&)=delete;\
class_name(class_name##&&)=delete;\
class_name##& operator=(class_name##&&)=delete;

namespace MetaInit {

	using vec2 = glm::vec2;
	using vec3 = glm::vec3;
	using vec4 = glm::vec4;

	using mat3 = glm::mat3;
	using mat4 = glm::mat4;

	using ivec2 = glm::ivec2;
	using ivec3 = glm::ivec3;
	using ivec4 = glm::ivec4;

	using uvec2 = glm::uvec2;
	using uvec3 = glm::uvec3;
	using uvec4 = glm::uvec4;

	using bvec2 = glm::bvec2;
	using bvec3 = glm::bvec3;
	using bvec4 = glm::bvec4;

	template<typename T>
	using Vector = eastl::vector<T>;
	template<typename T, uint32_t reverse_size=16, bool overflow=true>
	using SmallVector = eastl::fixed_vector<T, reverse_size, overflow>;
	template<typename Key, typename Val>
	using Map = eastl::hash_map<Key, Val>;
	template<typename Key, typename Val, uint32_t reverse_size, bool overflow=false>
	using SmallMap = eastl::fixed_hash_map < Key, Val, reverse_size, overflow> ;
	template<typename Val>
	using Set = eastl::hash_set<Val>;
	template<typename T>
	using Queue = eastl::queue<T>;
	template<typename T>
	using Stack = eastl::stack<T>;
	template<typename T>
	using List = eastl::list<T>;
	template<size_t N>
	using BitSet = eastl::bitset<N>;
	using BitVector = eastl::bitvector<>;

	template<typename T>
	using Span = eastl::span<T>;

	template<typename T>
	using Optional = eastl::optional<T>;

	using String = eastl::string;
	
	//multi thread 
	/*
	template<typename T>
	using Future = folly::Future<T>;

	template<typename T>
	using Promise = folly::Promise<T>;
	*/
}

namespace MetaInit::Utils {

	template <typename Enum>
	constexpr bool HasAnyFlags(Enum flags, Enum to_check) {
		return std::underlying_type_t<Enum>(flags) & std::underlying_type_t<Enum>(to_check) != 0;
	}
	template <typename Enum>
	constexpr Enum LogicAndFlags(Enum lhs, Enum rhs) {
		auto res = std::underlying_type_t<Enum>(lhs) & std::underlying_type_t<Enum>(rhs);
		return static_cast<Enum>(res);
	}
	template <typename Enum>
	constexpr Enum LogicOrFlags(Enum lhs, Enum rhs) {
		auto res = std::underlying_type_t<Enum>(lhs) | std::underlying_type_t<Enum>(rhs);
		return static_cast<Enum>(res);
	}

	template <typename T>
	requires std::is_integral_v<T>
	static inline constexpr bool IsPow2(T x) {
		return (x & (x - 1)) == 0;
	}

	template<typename T, typename U>
	requires std::is_integral_v<T> && std::is_integral_v<U>
	static inline constexpr T AlignDown(T val, U alignment) {
		return val & (~static_cast<T>(alignment - 1));
	}

	template<typename T, typename U>
	requires std::is_integral_v<T> && std::is_integral_v<U>
	static inline constexpr T AlignUp(T val, U alignment) {
		return (val + alignment - 1) & ~(static_cast<T>(alignment - 1));
	}

	template<class Atomic=std::atomic_bool>
	class SpinLock
	{
	public:
		SpinLock(Atomic& sign):sign_(sign) {
			bool expected = false;
			while (!sign_.compare_exchange_weak(expected, true)) {}
		}
		~SpinLock() {
			sign_.exchange(false);
		}
	private:
		Atomic& sign_;
	};
}

