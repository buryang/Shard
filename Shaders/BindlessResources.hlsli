#ifndef _BINDLESS_RESOURCES_INC_
#define _BINDLESS_RESOURCES_INC_

#include "Platform.hlsli"
/*
* according to REAC 2024, bindless only need to support for r/w views while constant/vertex/index 
* buffers are faster with direct bind(NVIDIA)
*/

//whether support globallycoherent memory
#define USE_GLOBALLY_CORHERENT 1

//space macro defines
#define SHARD_BINDLESS_TEXTURE_SPACE 0
#define SHARD_BINDLESS_BUFFER_SPACE 1
#define SHARD_BINDLESS_SAMPLER_SPACE 2

//non-bindless resource space 
#define SHARD_DEDICATE_SPACE_BEGIN 10

#ifndef __cplusplus
#include "CommonUtils.hlsli"

#define GLUE(str, n) str##n 
#define TEXTURE_SPACE GLUE(space, SHARD_BINDLESS_TEXTURE_SPACE)
#define BUFFER_SPACE GLUE(space, SHARD_BINDLESS_BUFFER_SPACE)
#define SAMPLER_SPACE GLUE(space, SHARD_BINDLESS_SAMPLER_SPACE)

/*
cbuffer BindLessConstants
{
    uint descriptor_table_offset0_;
    uint descriptor_table_offset1_;
    uint descriptor_table_offset2_;
    uint descriptor_table_offset3_;
    uint root_constant0_;
    uint root_constant1_;
    uint root_constant2_;
    uint root_constant3_;
};
*/
//for vulkan so we use array method for beblow SM6.6
#define BINDLESS_BEGIN_INDEX(bindless_handle_byte) (bindless_handle_byte >> 4)  //according to re-engine: u'd beter do manual scalarazation
StructuredBuffer<uint4> bindless_buffers : register(b0, BUFFER_SPACE);           //about 32 MiB according to re-engine : https://www.youtube.com/watch?v=7XWdiktsVyo
Texture2D bindless_textures2Ds[] : register(t0, TEXTURE_SPACE);
RWTexture2D bindless_rwtextures2Ds[] : register(u0, TEXTURE_SPACE);
RasterizerOrderedTexture2D bindless_rov_textures[] : register(t1, TEXTURE_SPACE);
ByteAddressBuffer bindless_byte_buffers[] : register(b1, BUFFER_SPACE);
RWByteAddressBuffer bindless_rwbyte_buffers[] : register(u1, BUFFER_SPACE);
SamplerState bindless_immutable_samplers[] : register(t0, SAMPLER_SPACE);

#define GetBindlessTexture2D(index) bindless_textures2Ds[NonUniformResourceIndex(index)]
#define GetBindlessROVTexture2D(index) bindless_rov_texture2Ds[NonUniformResourceIndex(index)]
#define GetBindlessByteBufferUniform(index) bindless_byte_buffers[index]
#define GetBindlessRWByteBufferUniform(index) bindless_rwbyte_buffers[index]
#define SampleBindlessTexture2D(tex_index, samp_index, uv) bindless_textures2Ds[NonUniformResourceIndex(tex_index)].Sample(bindless_immutable_samplers[NonUniformResourceIndex(samp_index)], uv)

#if USE_GLOBALLY_CORHERENT
/**
* globallycoherent is a modifier that can be applied to a resource (like a RWBuffer or RWTexture) 
* to ensure that writes to the resource are visible across all shader invocations in the entire GPU
*/
globallycoherent RWTexture2D bindless_coherent_rwtexture2D[] : register(u2, TEXTURE_SPACE);
globallycoherent RWByteAddressBuffer bindless_coherent_rwbyte_buffers[] : register(u3, BUFFER_SPACE);
#define GetCoherentBindlessRWTexture2D(index) bindless_coherent_rwtexture2Ds[index]
#define GetCoherentBindlessRWByteBuffer(index) bindless_coherent_rwbyte_buffers[index]
#endif

/**
*structure load/store help functions declare
*/
#define DECLARE_STRUCTURE_ACESS(struct_type, struct_size) \
inline void Store##struct_type(uint buffer_index, uint index, struct_type data) {\
    const uint offset = index * struct_size; \
    xx.Store##struct_size(offset, data);\
}\
inline struct_type Load##struct_type(uint buffer_index, uint index) {\
    const uint offset = index * struct_size; \
    return xx.Load##struct_size(offset);\
}

#endif

#endif //_BINDLESS_RESOURCES_INC_