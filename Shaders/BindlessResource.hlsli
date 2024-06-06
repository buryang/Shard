#ifndef _BINDLESS_RESOURCE_INC_
#define _BINDLESS_RESOURCE_INC_

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
StructuredBuffer<uint4> bindless_buffer : register(b0, BUFFER_SPACE);           //about 32 MiB according to re-engine : https://www.youtube.com/watch?v=7XWdiktsVyo
Texture2D bindless_textures2D[] : register(t0, TEXTURE_SPACE);
RWTexture2D bindless_rwtextures2D[] : register(u0, TEXTURE_SPACE);
RasterizerOrderedTexture2D bindless_rov_texture[] : register(t1, TEXTURE_SPACE);
ByteAddressBuffer bindless_byte_buffers[] : register(b1, BUFFER_SPACE);
RWByteAddressBuffer bindless_rwbyte_buffers[] : register(u1, BUFFER_SPACE);
SamplerState bindless_immutable_samplers[] : register(t0, SAMPLER_SPACE);

#define GetBindlessTexture2D(index) bindless_textures2D[NonUniformResourceIndex(index)]
#define GetBindlessROVTexture2D(index) bindless_rov_texture2D[NonUniformResourceIndex(index)]
#define GetBindlessByteBufferUniform(index) bindless_byte_buffers[index]
#define GetBindlessRWByteBufferUniform(index) bindless_rwbyte_buffers[index]

#endif

#endif //_BINDLESS_RESOURCE_INC_