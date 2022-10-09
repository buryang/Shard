#pragma once

//shader parameter define macros
#define BEGIN_SHADER_PARAMETER_STRUCT(T, Name, PREFIX, ...) \
PREFIX class T {											\	
private: static constexpr uint32_t counter_base_ = __COUNTER__;\
protected: \
template <uint32_t index> class InternalLinkType {\
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
