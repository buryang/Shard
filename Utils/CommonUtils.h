#pragma once

#ifdef WIN32
#define WINAPI __declspec(dllexport)
#else 
#define WINAPI
#endif 

#include "glm/glm.hpp"
#include "folly/FBVector.h"
#include "folly/small_vector.h"
#include <memory>
#include <vector>
#include <set>
#include <array>
#include <algorithm>

namespace MetaInit {

	template<typename Containers>
	using Vector = folly::fbvector<Containers>;
	template<typename Containers>
	using SmallVector = folly::small_vector<Containers>;
}