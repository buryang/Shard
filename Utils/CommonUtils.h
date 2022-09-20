#pragma once

#ifdef _MSC_VER
#define MINIT_API __declspec(dllexport)
#else 
#define MINIT_API
#endif 

#define FORCE_INLINE	__forceinline 
#define ALIGN_AS(align)	alignas(align) 

#define GLM_FORCE_CTOR_INIT
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "folly/FBVector.h"
#include "folly/FBString.h"
#include "folly/small_vector.h"
#include "folly/memory.h"
#include "folly/futures/Future.h"
#include "folly/hash/Hash.h"
#include "absl/types/span.h"
#include "absl/hash/hash.h"
#include "absl/types/optional.h"
#include <memory>
#include <vector>
#include <set>
#include <array>
#include <map>
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

	using uvec2 = glm::uvec2;
	using uvec3 = glm::uvec3;
	using uvec4 = glm::uvec4;

	using bvec2 = glm::bvec2;
	using bvec3 = glm::bvec3;
	using bvec4 = glm::bvec4;

	template<typename T>
	using Vector = folly::fbvector<T>;
	template<typename T, int reverse_size=16>
	using SmallVector = folly::small_vector<T, reverse_size>;

	template<typename T>
	using List = std::list<T>;

	template<typename T>
	using Span = absl::Span<T>;

	template<typename T>
	using Optional = absl::optional<T>;

	using String = folly::fbstring;
	
	//multi thread 
	template<typename T>
	using Future = folly::Future<T>;

	template<typename T>
	using Promise = folly::Promise<T>;

}

namespace MetaInit::Utils {

	//range for helper function
	class Range
	{
	public:
		Range(uint32_t begin, uint32_t end) :begin_(begin), end_(end) {
			assert(end_ > begin_ && begin_ >= 0);
		}
		template<typename LAMBDA>
		void For(LAMBDA&& lambda)const {
			for (auto n = begin_; n < end_; ++n) {
				lambda(n);
			}
		}
	private:
		const uint32_t	begin_{ 0 };
		const uint32_t	end_{ 0 };
	};

	template <typename Enum>
	bool HasAnyFlags(Enum flags, Enum to_check) {
		return std::underlying_type_t<Enum>(flags) & std::underlying_type_t<Enum>(to_check);
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

