#pragma once

#ifdef WIN32
#define WINAPI __declspec(dllexport)
#else 
#define WINAPI
#endif 

#include "glm/glm.hpp"
#include "folly/FBVector.h"
#include "folly/small_vector.h"
#include "absl/types/span.h"
#include <memory>
#include <vector>
#include <set>
#include <array>
#include <algorithm>

#define DISALLOW_COPY_AND_ASSIGN(class_name) \
class_name(const class_name##&)=delete; \
class_name##& operator=(const class_name##&)=delete;

namespace MetaInit {

	template<typename Containers>
	using Vector = folly::fbvector<Containers>;
	template<typename Containers>
	using SmallVector = folly::small_vector<Containers>;
	
	template<typename Containers>
	using Span = absl::Span<Containers>;
}