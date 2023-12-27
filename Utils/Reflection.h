#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Handle.h"
#include "Utils/Hash.h"
#include "Utils/ReflectionImageObject.h"
#include <cstddef>

//more information from https://shaharmike.com/cpp/vtable-part3/
namespace Shard::Utils {
     enum ELayoutFlags {
          eNone = 0x0,
          eInitialized = 0x1,
          e32Bit = 0x2,
          eAlignBases = 0x4,//base class should to be padded to align size
          eRayTracing = 0x8,
          //todo other
          eAllIn = 0xFFFFFFFF
     };

     class PlatformTypeLayoutParameters
     {
     public:
          PlatformTypeLayoutParameters() = default;
          static PlatformTypeLayoutParameters& CurrentParameters() {
               static PlatformTypeLayoutParameters curr_paramters;
               if (!curr_paramters.IsValid()) {
                    //do init work here
                    auto& flags = curr_paramters.flags_;
                    flags = ELayoutFlags::eInitialized;
                    if (sizeof(void*) == 4) {
                         flags |= ELayoutFlags::e32Bit;
#ifdef WIN32
                         curr_paramters.max_alignment_ = 4u;
                    }
                    else {
                         curr_paramters.max_alignment_ = 8u;
#endif
                    }
#ifdef RAY_TRACING_ENABLED
                    flags |= ELayoutFlags::eRayTracing;
#endif
               }
               return curr_paramters;
          }
          static bool IsCurrentPlatform(const PlatformTypeLayoutParameters& rhs) {
               return CurrentParameters() == rhs;
          }
          FORCE_INLINE bool Is32bit() const {
               return flags_ & ELayoutFlags::e32Bit;
          }
          FORCE_INLINE bool IsRayTracingEnable() const {
               return flags_ & ELayoutFlags::eRayTracing != 0u;
          }
          FORCE_INLINE bool IsAlignBasesEnable() const {
               return flags_ & ELayoutFlags::eAlignBases != 0u;
          }
          FORCE_INLINE bool IsValid()const {
               return flags_ != 0;
          }
          FORCE_INLINE bool IsCurrentPlatform()const {
               return *this == CurrentParameters();
          }
          FORCE_INLINE uint32_t GetFlags()const {
               return flags_;
          }
          FORCE_INLINE uint32_t GetPointerSize() const {
               return Is32bit() ? sizeof(uint32_t) : sizeof(uint64_t);
          }
          FORCE_INLINE uint32_t GetMaxAlignment() const {
               return max_alignment_;
          }
          FORCE_INLINE void SetFlags(uint32_t flags) {
               flags_ = flags;
          }
          friend FORCE_INLINE bool operator==(const PlatformTypeLayoutParameters& lhs, const PlatformTypeLayoutParameters& rhs) {
               return lhs.flags_ == rhs.flags_ && lhs.max_alignment_ == rhs.max_alignment_;
          }
          friend FORCE_INLINE bool operator!=(const PlatformTypeLayoutParameters& lhs, const PlatformTypeLayoutParameters& rhs) {
               return !(lhs == rhs);
          }
          friend FileArchive& operator <<(FileArchive& ar, PlatformTypeLayoutParameters& ref) {
               ar << ref.flags_ << ref.max_alignment_;
               return ar;
          }
     private:
          uint32_t     flags_{ 0u };
          uint32_t     max_alignment_{ 0xFFFFFFFF };
     };

     struct TypeLayoutDesc;

     //utility function define
     void DefaultLayoutDestroy(void* object, const TypeLayoutDesc& desc, bool is_frozen);
     uint32_t DefaultLayoutWrite(ImageWriter& writer, const void* object, const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc);
     uint32_t DefaultLayoutHashAppend(const TypeLayoutDesc& desc, const PlatformTypeLayoutParameters& platform, blake3_hasher& hasher);
     uint32_t DefaultLayoutGetAlignment(const TypeLayoutDesc& desc, const PlatformTypeLayoutParameters& platform);
     uint32_t DefaultLayoutFreeze(const void* object, const TypeLayoutDesc& desc, const PlatformTypeLayoutParameters& frozen_platform);
     uint32_t DefaultUnfreezeCopy(const void* object, const TypeLayoutDesc& desc, const PlatformTypeLayoutParameters& frozen_platform, void* dst_obj);
     uint32_t DefaultGetOffsetFromBase(const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc);
     bool TryToGetOffsetFromBase(const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc, uint32_t& offset);

     struct FieldLayoutDesc
     {
          String     name_;
          uint32_t     index_{ 0u };
          uint32_t     flags_{ eNone }; 
          uint32_t     offset_{ 0u };
          /*Adjacent bit - field members may(or may not) be packed
           *to share and straddle the individual bytes. only for integral type
           */
          uint32_t     bit_size_{ 0u };
          uint32_t     array_size_{ 1u };
          TypeLayoutDesc&     type_;
          static bool IsFieldIncluded(const FieldLayoutDesc& desc, const PlatformTypeLayoutParameters& platform);
          static FORCE_INLINE bool IsValid(const FieldLayoutDesc& desc) {
               return desc.flags_ == ELayoutFlags::eNone;
          }
     };

     enum class EClassInterfaceType {
          eNonVirtual,
          eVirtual,
          eAbstract,
     };

     static FORCE_INLINE bool IsNonVirtualClass(EClassInterfaceType type) {
          return type == EClassInterfaceType::eNonVirtual;
     }

     struct TypeLayoutDesc {
          
          uint32_t     align_{ 0u };
          uint32_t     size_{ 0u };
          uint32_t     size_from_fields_{ ~0u };
          Blake3Hash64     hash_name_;
          EClassInterfaceType     interface_{ EClassInterfaceType::eNonVirtual };
          uint32_t     num_bases_{ 0u };//base class count
          uint32_t     num_vbases_{ 0u }; //virtual base class count
          SmallVector<FieldLayoutDesc> fields_;

          //utility function pointer define
          using LayoutDestroyFunc = std::function<void(void* object, const TypeLayoutDesc& desc, bool is_frozen)>;
          using LayoutWriteFunc = std::function<uint32_t(ImageWriter& writer, const void* object, const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc)>;
          using LayoutHashAppendFunc = std::function<uint32_t(const TypeLayoutDesc& desc, const PlatformTypeLayoutParameters& platform, blake3_hasher& hasher)>;
          using LayoutGetAlignmentFunc = std::function<uint32_t(const TypeLayoutDesc& desc, const PlatformTypeLayoutParameters& platform)>;
          using LayoutUnfreezeCopyFunc = std::function<uint32_t(const void* object, const TypeLayoutDesc& desc, const PlatformTypeLayoutParameters& platform, void* dst_obj)>;
          using LayoutGetOffsetFromBaseFunc = std::function<uint32_t(const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc)>;
          using LayoutGetDefaultInstanceFunc = std::function<void*()>;

          LayoutWriteFunc     write_func_{ DefaultLayoutWrite };
          LayoutDestroyFunc     destroy_func_{ DefaultLayoutDestroy };
          LayoutHashAppendFunc     hash_func_{ DefaultLayoutHashAppend };
          LayoutGetAlignmentFunc     alignment_func_{ DefaultLayoutGetAlignment };
          LayoutUnfreezeCopyFunc     unfreeze_copy_func_{ DefaultUnfreezeCopy };
          LayoutGetOffsetFromBaseFunc     offset_to_base_func_{ DefaultGetOffsetFromBase };
          LayoutGetDefaultInstanceFunc     default_instance_func_{ nullptr };
          static bool InitializeTypeLayout(TypeLayoutDesc& desc, const PlatformTypeLayoutParameters& platform);
          static bool RegistTypeLayout(const TypeLayoutDesc& type_layout);
          static TypeLayoutDesc* GetTypeLayout(Blake3Hash64 hash);
          static uint32_t GetOffsetFromBase(const TypeLayoutDesc& derived, const TypeLayoutDesc& base);
          static bool IsDerivedFrom(const TypeLayoutDesc& probe_derived, const TypeLayoutDesc& probe_base);
          static bool IsEmptyType(const TypeLayoutDesc& desc);
     };

     bool operator==(const TypeLayoutDesc& lhs, const TypeLayoutDesc& rhs) {
          return false; //todo 
     }

     bool operator!=(const TypeLayoutDesc& lhs, const TypeLayoutDesc& rhs) {
          return !(lhs == rhs);
     }

     //for some type use self defined string to hash, so not use typeinfo.hash_code
     static inline constexpr Blake3Hash64 CalcStringHash(const String& str) {
          blake3_hasher hasher;
          blake3_hasher_init(&hasher);
          blake3_hasher_update(&hasher, str.data(), str.size());
          Blake3Hash64 hash_name;
          blake3_hasher_finalize(&hasher, hash_name.GetBytes(), hash_name.GetHashSize());
          return hash_name;
     }

     template<typename T>
     uint32_t PrimitiveLayoutWrite(ImageWriter& writer, const void* object, const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc) {
          writer.WriteBytes(object, sizeof(T));
          return sizeof(T);
     }

     template<typename T>
     uint32_t PrimitiveUnfreezeCopy(const void* object, const TypeLayoutDesc& desc, const PlatformTypeLayoutParameters& frozen_platform, void* dst_obj) {
          new (dst_obj)T(object);
          return sizeof(T);
     }

     template<typename T>
     uint32_t PrimitiveHashAppend(const TypeLayoutDesc& desc, blake3_hasher& hasher) {
          const auto offset = sizeof(T);
          blake3_hasher_update(&hasher, &offset, sizeof(offset)); //to fix
          return offset;
     }

     template<typename Element>
     uint32_t VectorLayoutWrite(ImageWriter& writer, Vector<Element>* object, const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc) {
          writer.WriteRawPointerSizedBytes(-1);
          writer.WriteObjectArray(, desc, );
     }

     template <typename T, typename = std::enable_if_t<!std::is_arithmetic_v<T>, void>>
     TypeLayoutDesc& GetPrimitiveLayout(){
          LOG(ERROR) << "not support primitive reflection";
     }

     template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>, void>>
     TypeLayoutDesc& GetPrimitiveLayout() {
          static TypeLayoutDesc arithmetic_desc{ .hash_name_ = CalcStringHash(std::typeid(T).name), 
                                                       .align_=alignof(T), .size_ = sizeof(T),
                                                       .hash_func_ = PrimitiveHashAppend<T>,
                                                       .write_func_ = PrimitiveLayoutWrite<T>,
                                                       .unfreeze_copy_func_ = PrimitiveUnfreezeCopy<T> };
          return arithmetic_desc;
     }

#define DECLARE_DEFAULT_POD_TYPE_LAYOUT(TYPE)\
     template <> \
     TypeLayoutDesc& GetPrimitiveLayout<TYPE, void>() {\
          static TypeLayoutDesc pod_desc{ .hash_name_ = CalcStringHash(std::typeid(TYPE).name), \
                                                       .align_=alignof(T), .size_ = sizeof(TYPE),\
                                                       .hash_func_=PrimitiveHashAppend<TYPE>,\
                                                       .write_func_ = PrimitiveLayoutWrite<TYPE>,\
                                                       .unfreeze_copy_func_ = PrimitiveUnfreezeCopy<TYPE> };\
          return pod_desc;\
     }

     template <typename T>
     TypeLayoutDesc& GetPrimitiveLayout<Vector<T>, void>() {
          static TypeLayoutDesc vector_desc{ .hash_name_ = CalcStringHash("Vector<" + std::typeid(T).name + ">"),
                                                       .align_ = alignof(Vector<T>),
                                                       .size_ = sizeof(Vector<T>),
                                                       .fields_ = {TypeLayoutResolver<T>.Get()},
                                                       .write_func_ = VectorLayoutWrite<T> };
          return vector_desc;
     }

     /*
     template <typename T>
     TypeLayoutDesc& GetPrimitiveLayout<SmallVector<T>, void>() {
          static TypeLayoutDesc vector_desc{ .hash_name_ = CalcStringHash("SmallVector<" + std::typeid(T).name + ">"),
                                                  .align_ = alignof(SmallVector<T>), 
                                                  .size_ = sizeof(SmallVector<T>),
                                                  .fields_ = {TypeLayoutResolver<T>.Get()} };
          return vector_desc;
     }
     */

     template <>
     TypeLayoutDesc& GetPrimitiveLayout<String, void>(){
          static TypeLayoutDesc string_desc{ .hash_name_ = CalcStringHash("String"), 
                                                  .align_ = alignof(String),
                                                  .size_ = sizeof(String),
                                                  .hash_func_ = nullptr,
                                                  .write_func_ = nullptr,
                                                  .unfreeze_copy_func_ = nullptr };
          return string_desc;
     }

     template<typename, typename = void>
     struct HasLayoutDef : std::false_type {};

     template<typename T>
     struct HasLayoutDef<T, std::void_t<decltype(&T::g_static_desc)> > : std::true_type {};

     struct TypeLayoutResolver {
          template<typename T, typename = std::enable_if_t<HasLayoutDef<T>::value>>
          static TypeLayoutDesc& Get(){
               return T::g_static_desc;
          }
          template<typename T, typename = std::enable_if_t<!HasLayoutDef<T>::value>>
          static TypeLayoutDesc& Get() {
               return GetPrimitiveLayout<T>();
          }
     };
}

using namespace Shard::Utils;
//***macros that layout regist needed***
#define     STATIC_DESC_DECLARE()\
static TypeLayoutDesc g_static_desc; \
template<uint32_t index> \
static bool InternalLinkFunc() { LOG(ERROR)<<"typedesc failed to init"; }

#define INTERNAL_TYPE_CONSTRUCTOR_DEFAULT(TYPE, COUNTER)\
template<> static bool InternalLinkFunc<COUNTER>() { \
{\
     g_static_desc.hash_name_ = CalcStringHash(#TYPE);\
     g_static_desc.size_ = sizeof(TYPE) };\
     return InternalLinkFunc<COUNTER+1>(); \
}

#define TYPE_CONSTRUCTOR_DEFAULT(TYPE)\
STATIC_DESC_DECLARE();\
INTERNAL_TYPE_CONSTRUCTOR_DEFAULT(TYPE, 0);

#define INTERNAL_TYPE_CONSTRUCTOR_WITH_INTERFACE(TYPE, COUNTER, ...)\
template<> static bool InternalLinkFunc<COUNTER>() { \
{\
     g_static_desc.hash_name_ = CalcStringHash(#TYPE);\
     g_static_desc.size_ = sizeof(TYPE) };\
     g_static_desc..interface_ = __VA_ARGS__;\
     return InternalLinkFunc<COUNTER+1>(); \
}

#define TYPE_CONSTRUCTOR_WITH_INTERFACE(TYPE, ...)\
STATIC_DESC_DECLARE();\
INTERNAL_TYPE_CONSTRUCTOR_WITH_INTERFACE(TYPE, 0, __VA_ARGS__);

//puzzled from https://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros
#define FUNCTION_CHOOSER(FUNC0, FUNC1, ...) FUNC1
#define FUNCTION_RECOMPOSER(XXX) FUNCTION_CHOOSER XXX
#define CHOOSE_FROM_ARG_COUNT(...) FUNCTION_RECOMPOSER((__VA_ARGS__, TYPE_CONSTRUCTOR_WITH_INTERFACE, ))
#define NO_ARG_EXPANDER() ,TYPE_CONSTRUCTOR_DEFAULT
#define TYPE_CONSTRUCTOR_CHOOSE(TYPE, ...) CHOOSE_FROM_ARG_COUNT(NO_ARG_EXPANDER __VA_ARGS__ ())

#define BEGIN_DECLARE_TYPE_LAYOUT_DEF(TYPE, ...)\
public:\
     using ThisType = TYPE; \
     static constexpr uint32_t g_index_base{__COUNTER__};\
     TYPE_CONSTRUCTOR_CHOOSE(TYPE, __VA_ARGS__)(TYPE, __VA_ARGS__);\
     static bool TypeLayoutDescAddField(const String& field_name, uint32_t index, uint32_t offset, uint32_t array_size, TypeLayoutDesc& field_type) \
     { \
          FieldLayoutDesc field_desc{ .index_ = index, .name_ = field_name, .offset_=offset, .type_ = field_type}; \
          g_static_desc.fields_.emplace_back(field_desc); \
          return true; \
     } \
     template<class BASE_TYPE> \
     static bool TypeLayoutDescAddBaseClassField(uint32_t index) \
     { \
          ThisType temp; \
          const auto offset = (uint32_t)((uint64_t)static_cast<BASE_TYPE*>(&temp)-(uint64_t)(&temp)); \
          FieldLayoutDesc base_desc{.index_ = index, .name_ = "BASE", .offset_=offset, .type_ = TypeLayoutResolver::Get<BASE_TYPE>() }; \
          g_static_desc.fields_.emplace_back(base_desc); \
          ++g_static_desc.num_bases_; \
          return true; \
     } \
     static const ThisType* GetDefaultEntity(){ \
          static const ThisType instance; \
          return &instance; \
     }

#define END_DECLARE_TYPE_LAYOUT_DEF(TYPE)\
public:\
     template<> static bool InternalLinkFunc<__COUNTER__ - g_index_base>(){\
          TypeLayoutDesc::InitializeTypeLayout(g_static_desc, PlatformTypeLayoutParameters::CurrentParameters()); \
          return true;\
     }

//local type to avoid "::" 
#define IMPLEMENT_TYPE_LAYOUT_DEF(TYPE)\
     namespace { using LOCAL_TYPE = TYPE; static const auto g_return_dummy = LOCAL_TYPE::InternalLinkFunc<0>(); }

/*for each base class*/
#define INTERNAL_DECLARE_BASE_TYPE_LAYOUT(BASE_TYPE, COUNTER)\
     template<> static bool InternalLinkFunc<COUNTER>()\
          TypeLayoutDescAddBaseClassField<BASE_TYPE>(COUNTER-1);\
          return InternalLinkFunc<COUNTER+1>();\
     }

#define DECLARE_BASE_TYPE_LAYOUT_FUNC(BASE_TYPE)\
     INTERNAL_DECLARE_BASE_TYPE_LAYOUT(BASE_TYPE, __COUNTER__ - g_index_base);

#define DECLARE_BASE_TYPE_LAYOUT_EXPLCIT(...)\
     FOR_EARCH(DECLARE_BASE_TYPE_LAYOUT_FUNC, __VA_ARGS__)     

//counter - 1 for base from 0
#define INTERNAL_LAYOUT_FIELD(TYPE, NAME, OFFSET, COUNTER, ARRAY_SIZE)\
     template<> static bool InternalLinkFunc<COUNTER>(){\
          TypeLayoutDescAddField(#NAME, COUNTER-1, OFFSET, ARRAY_SIZE, TypeLayoutResolver::Get<TYPE>());\
          return InternalLinkFunc<COUNTER+1>();\
     }

#define LAYOUT_FIELD(PREFIX, TYPE, NAME)\
     PREFIX TYPE NAME; \
     INTERNAL_LAYOUT_FIELD(TYPE, NAME, std::offsetof(ThisType, NAME), __COUNTER__-g_index_base, 1);

#define LAYOUT_FIELD_DEFAULT(PREFIX, TYPE, NAME, VALUE)\
     PREFIX TYPE NAME{VALUE}; \
     INTERNAL_LAYOUT_FIELD(TYPE, NAME, std::offsetof(ThisType, NAME), __COUNTER__-g_index_base, 1);

#define LAYOUT_VECTOR_FIELD(PREFIX, TYPE, NAME)\
     PREFIX Vector<TYPE>     NAME; \
     INTERNAL_LAYOUT_FIELD(TYPE, NAME, std::offsetof(ThisType, NAME), __COUNTER__-g_index_base, 1);

#define LAYOUT_ARRAY_FIELD(PREFIX, TYPE, NAME, ARRAY_SIZE)\
     PREFIX TYPE NAME[ARRAY_SIZE]; \
     INTERNAL_LAYOUT_FIELD(TYPE, NAME, std::offsetof(ThisType, NAME), __COUNTER__-g_index_base, ARRAY_SIZE);

/*for cannot get bit-filed offset from class*/
#define LAYOUT_BIT_FIELD(PREFIX, TYPE, NAME, BIT_SIZE)\
     PREFIX TYPE NAME:BIT_SIZE; \
     NTERNAL_LAYOUT_FIELD(TYPE, NAME, -1, __COUNTER__-g_index_base, 1);

#if 0
//macro usage example
namespace Shard {
     class Usage {
          BEGIN_DECLARE_TYPE_LAYOUT_DEF(Usage);
          LAYOUT_FIELD(, uint32_t, dummy_);
          END_DECLARE_TYPE_LAYOUT_DEF();
     };
     IMPLEMENT_TYPE_LAYOUT_DEF(Usage);
}
#endif

#ifdef RAY_TRACING //to do
#define LAYOUT_FIELD_RAYTRACING(PREFIX, TYPE, NAME) 
#define LAYOUT_FIELD_RAYTRACING_DEFAULT(PREFIX, TYPE, NAME, VALUE)
#else
#define LAYOUT_FIELD_RAYTRACING(PREFIX, TYPE, NAME) 
#define LAYOUT_FIELD_RAYTRACING_DEFAULT(PREFIX, TYPE, NAME, VALUE)
#endif
