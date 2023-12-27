#pragma once

#if __cplusplus < 202002L
     #error only support c++ version above c++20
#endif 

#ifdef _MSC_VER
     #define MINIT_API __declspec(dllexport)
     #define GLOG_NO_ABBREVIATED_SEVERITIES
#else 
     #define MINIT_API
#endif 


#define FORCE_INLINE     __forceinline 
#define ALIGN_AS(align)     alignas(align) 
#define ALIGN_CACHELINE alignas(std::hardware_destructive_interference_size)

#pragma warning(push)
#pragma warning(disable:4996)

#define GLM_FORCE_CTOR_INIT
#include "glm/glm.hpp"
#include "glm/ext.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glog/logging.h"
#include "eastl/vector.h"
#include "eastl/fixed_vector.h"
#include "eastl/hash_map.h"
#include "eastl/fixed_hash_map.h"
#include "eastl/hash_set.h"
#include "eastl/fixed_hash_set.h"
#include "eastl/list.h"
#include "eastl/queue.h"
#include "eastl/stack.h"
#include "eastl/span.h"
#include "eastl/optional.h"
#include "eastl/string.h"
#include "EASTL/sort.h"
#include "eastl/bitset.h"
#include "eastl/bitvector.h"
#include "eastl/unique_ptr.h"
#include "eastl/shared_ptr.h"
#include "eastl/internal/thread_support.h"
#include "eastl/algorithm.h"
#include "folly/Poly.h" 
#include "fmt/format.h"
#include <memory>
#include <algorithm>
#include <cassert>
#ifdef _WIN32
#include <windows.h>
#else
#include <codecvt>
#endif

#pragma warning(pop)

#define DISALLOW_COPY_AND_ASSIGN(class_name) \
class_name(const class_name&)=delete; \
class_name& operator=(const class_name&)=delete; \
class_name(class_name&&)=delete; \
class_name& operator=(class_name&&)=delete;

/*foreach varargs macro*/
#define PARENS ()

#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define FOR_EACH(MACRO, ...)                                    \
  __VA_OPT__(EXPAND(FOR_EACH_HELPER(MACRO, __VA_ARGS__)))
#define FOR_EACH_HELPER(MACRO, A1, ...)                         \
  MACRO(A1)                                                     \
  __VA_OPT__(FOR_EACH_AGAIN PARENS (MACRO, __VA_ARGS__))
#define FOR_EACH_AGAIN() FOR_EACH_HELPER

namespace Shard {

     using glm::vec2;
     using glm::vec3;
     using glm::vec4;

     using glm::mat3;
     using glm::mat4;

     using glm::ivec2;
     using glm::ivec3;
     using glm::ivec4;

     using glm::uvec2;
     using glm::uvec3;
     using glm::uvec4;

     using glm::bvec2;
     using glm::bvec3;
     using glm::bvec4;

     using glm::quat;

     using size_type = std::size_t;

     template<typename T, typename Allocator = eastl::allocator>
     using Vector = eastl::vector<T, Allocator>;
     template<typename T, uint32_t reverse_size=16, bool overflow=true, typename Allocator = eastl::allocator>
     using SmallVector = eastl::fixed_vector<T, reverse_size, overflow, Allocator>;
     template<typename T, uint32_t reverse_size>
     using Array = SmallVector<T, reverse_size, false, eastl::dummy_allocator>;
     template<typename Key, typename Val>
     using Map = eastl::hash_map<Key, Val>;
     template<typename Key, typename Val, uint32_t reverse_size, bool overflow=false>
     using FixedMap = eastl::fixed_hash_map < Key, Val, reverse_size, overflow> ;
     template<typename Val>
     using Set = eastl::hash_set<Val>;
     template<typename Value, size_t node_count>
     using FixedSet = eastl::fixed_hash_set<Value, node_count>;
     template<typename T>
     using Queue = eastl::queue<T>;
     template<typename T>
     using Stack = eastl::stack<T>;
     template<typename T>
     using List = eastl::list<T>;
     template<size_t N>
     using BitSet = eastl::bitset<N>;
     template<class Allocator>
     using BitVector = eastl::bitvector<Allocator>;

     template<typename T>
     using Span = eastl::span<T>;

     template<typename T>
     using Optional = eastl::optional<T>;

     using String = eastl::string;
     using WString = eastl::wstring;

#ifdef _UNICODE
     using Tchar = wchar_t;
     using TString = WString;
#define STR_PPEFIX(str) L##str
#else
     using Tchar = char;
     using TString = String;
#define STR_PREFIX(str) str
#endif
     
     //multi thread 
     /*
     template<typename T>
     using Future = folly::Future<T>;

     template<typename T>
     using Promise = folly::Promise<T>;
     */

     //poly tickable
     struct TickAbleInterface {
          template<class Base> struct Interface : Base {
              void Tick(float time) { folly::poly_call<0>(*this, time); }
          };
          template <class T>
          using Members = folly::PolyMembers<&T::Tick>;
     };
     using TickAble = folly::Poly<TickAbleInterface>;
}

namespace Shard::Utils {

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

     template <typename Enum>
     constexpr auto EnumToInteger(Enum enum_var) {
          return std::underlying_type_t<Enum>(enum_var);
     }

     template <typename T>
     requires std::is_integral_v<T>
          static inline constexpr bool IsPow2(T x) {
          return (x & (x - 1)) == 0;
     }

     template<typename T, typename U>
     requires std::is_integral_v<T>&& std::is_integral_v<U>
          static inline constexpr T AlignDown(T val, U alignment) {
          return val & (~static_cast<T>(alignment - 1));
     }

     template<typename T, typename U>
     requires std::is_integral_v<T>&& std::is_integral_v<U>
          static inline constexpr T AlignUp(T val, U alignment) {
          return (val + alignment - 1) & ~(static_cast<T>(alignment - 1));
     }

     template<class AtomicBOOL = std::atomic_bool>
     class SpinLock
     {
     public:
          //https://rigtorp.se/spinlock/
          SpinLock(AtomicBOOL& sign) :sign_(sign) {
               for (;;) {
                    bool expected = false;
                    if (LIKELY(sign_.compare_exchange_weak(expected, true, std::memory_order::acquire))) {
                         return;
                    }
                    auto spin_count = max_spin_count;
                    while(--spin_count > 0 && !sign_.load(std::memory_order::relaxed)){
                         _mm_pause(); //https://stackoverflow.com/questions/7488196/what-is-the-different-of-busy-loop-with-sleep0-and-pause-instruction
                    }
                    //slow lock
                    if (UNLIKELY(!spin_count)) {
                         std::this_thread::yield();
                    }
               }
          }
          ~SpinLock() {
               sign_.store(false, std::memory_order::release);
          }
     private:
          static constexpr uint32_t max_spin_count = 40u;
          AtomicBOOL& sign_;
     };

     //string/wstring conver helper
     class StringConvertHelper
     {
     public:
#ifdef _WIN32
          static inline WString StringToWString(const String& str) {
               WString wstr;
               auto num = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
               if (num > 0)
               {
                    wstr.resize(size_t(num) - 1);
                    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], num);
               }
          }
          static inline String WStringToString(const WString& wstr) {
               String str;
               auto num = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
               if (num > 0)
               {
                    str.resize(size_t(num));
                    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], num, nullptr, nullptr);
               }

          }
          static inline int CharToWChar(const char* from, wchar_t* to) {
               int num = MultiByteToWideChar(CP_UTF8, 0, from, -1, nullptr, 0);
               if (num > 0)
               {
                    MultiByteToWideChar(CP_UTF8, 0, from, -1, &to[0], num);
               }
               return num;
          }
          static inline int WCharToChar(const wchar_t* from, char* to) {
               int num = WideCharToMultiByte(CP_UTF8, 0, from, -1, nullptr, 0, nullptr, nullptr);
               if (num > 0)
               {
                    WideCharToMultiByte(CP_UTF8, 0, from, -1, &to[0], num, nullptr, nullptr);
               }
               return num;
          }
#else
          static inline WString StringToWString(const String& str) {
               std::wstring_convert<std::codecvt_utf8<wchar_t>> cv:
               return cv.from_bytes(str);
          }
          static inline String WStringToString(const WString& wstr) {
               std::wstring_convert<std::codecvt_utf8<wchar_t>> cv:
               return cv.to_bytes(wstr);
          }
          static inline int CharToWChar(const char* from, wchar_t* to) {
               std::wstring_convert<std::codecvt_utf8<wchar_t>> cv;
               std::memcpy(to, cv.from_bytes(from).c_str(), cv.converted());
               return (int)cv.converted();
          }
          static inline int WCharToChar(const wchar_t* from, char* to) {
               std::wstring_convert<std::codecvt_utf8<wchar_t>> cv;
               std::memcpy(to, cv.to_bytes(from).c_str(), cv.converted());
               return (int)cv.converted();
          }
#endif
     };

}

