#ifndef _PLATFORM_INC_
#define _PLATFOTM_INC_

#if defined(__DXC__) && defined(__spirv__) //dxc inner define for spir-v
#define VULKAN_BACKEND
#endif

//todo DXIL backend

#ifdef __DXC__ //for dxc compile
#define SM_MAJOR __SHADER_TARGET_MAJOR
#define SM_MINOR __SHADER_TARGET_MINOR
#endif 

#ifndef SM_MAJOR 
#define SM_MAJOR 6 //default enable shader model 6
#define SM_MINOR 0
#endif

#if SM_MAJOR < 5
#error "shader model 5 or higher is required"
#endif

#ifdef VULKAN_BACKEND //for DXC compile to spir-v
#define VK_PUSH_CONSTANT [[vk::push_constant]]
#define VK_BINDING(BIND, SET) [[vk::binding(BIND, SET)]]
#define VK_LOCATION(INDEX) [[vk::location(INDEX)]]
#define VK_CONSTANT(INDEX) [[vk::constant_id(INDEX)]]
#define VK_COUNTER(INDEX) [[vk::counter_binding(INDEX)]]
//https://github.com/Microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst#subpass-inputs
#define VK_INPUT_AATTACHMENT(ATTACH) [[vk::input_attachment_index(ATTACH)]]
#else 
#define VK_PUSH_CONSTANT
#define VK_BINDING(BIND, SET)
#define VK_LOCATION(INDEX)
#define VK_CONSTANT(INDEX)
#define VK_COUNTER(INDEX)
#define VK_INPUT_AATTACHMENT(ATTACH)
#endif

//numeric attributes
#if SM_MAJOR >= 6 && SM_MINOR >= 2 
#pragma require Native16Bit
#endif

//Pre-SM5 GPUs (e.g., DirectX 10/11-era hardware) have stricter constraints on register usage and 
//control flow. Forcing [unroll] could lead to register spills or bloated code, harming performance
#if SM_MAJOR >= 5 || COMPILER_SUPPORTS_ATTRIBUTES
#define UNROLL [unroll]
#define UNROLL_N(N) [unroll(N)]
#define LOOP [loop]
#define BRANCH [branch]
#define FLATTEN [flatten]
#define ALLOW_UAV_CONDITION [allow_uav_condition]
#else
#define UNROLL
#define UNROLL_N(N)
#define LOOP
#define BRANCH
#define FLATTEN
#define ALLOW_UAV_CONDITION 
#endif 

#define LIKELY [likely]
#define UNLIKELY [unlikely]
#define FASTOPT [fastopt]

//whether platform has wave intrinsics
#if SM_MAJOR >= 6
#define WAVE_INTRINSICS_ENABLED  1
#define PACKING_INTRINSICS_ENABLED 1
#define QUAD_INTRINSICS_ENABLED 1
#define BITFIELD_INTRINSICS_ENABLED 0
#define TEXEL_GATHER_ENABLED 1 //raw texture gather supported in shaer model 6.7
#define ATOMICS_CAS_64BIT_ENABLED 1 //supported in shader model 6.6
//whether platform support hitile lookup
#define HITILE_LOOKUP_INTRINSICS_ENABLED 1
#define VERTEX_SHADER_ATOMIC_UINT_ENABLED 1 //supported in shader model 6.6
#define VERTEX_SHADER_SRVS_ENABLED 1 
#else
#define WAVE_INTRINSICS_ENABLED  0
#define PACKING_INTRINSICS_ENABLED 0
#define QUAD_INTRINSICS_ENABLED 0
#define BITFIELD_INTRINSICS_ENABLED 1
#define TEXEL_GATHER_ENABLED 0
#define ATOMICS_CAS_64BIT_ENABLED 0
//whether platform support hitile lookup
#define HITILE_LOOKUP_INTRINSICS_ENABLED 0
#define VERTEX_SHADER_ATOMIC_UINT_ENABLED 0
#define VERTEX_SHADER_SRVS_ENABLED 1 
#endif

#define COMPILER_SUPPORT_MINMAX3 0

#ifdef GPU_VENOR_AMD //defined(__AMDGCN__) || defined(__MESA__)
#define THREAD_WARP_SIZE 64
#else //NVIDIA
#define THREAD_WARP_SIZE 32
#endif

#define WorkDistributionQueue RWByteAddressBuffer
#define WorkDistributionStateQueue RWByteAddressBuffer
#define RWCoherentByteAddressBuffer globallycoherent RWByteAddressBuffer
#define RWCoherentStructuredBuffer(structure) globallycoherent RWStructuredBuffer<structure>


#if !BITFIELD_INTRINSICS_ENABLED
//emulate of the low-level RDNA bfe/bfi intrinsics
uint bfe_u32(uint value, uint offset, uint bits)
{
    uint mask = (1U << bits) - 1U;
    uint shift_value = value >> offset;
    return shift_value & bits;
}
 
uint bfi_b32(uint value, uint preserve_mask, uint insert_mask)
{
    return (value & preserve_mask) | (~value & insert_mask);
    //todo for nvidia turing/ampere+ (lop3_lut((s0 << (s3 & 31)) | (s1 >> (32 - (s3 & 31))), s2, 0))
}
#endif 

//simulate some ds_swizzle_b32 operations
#ifndef COMPILER_WITH_SWIZZLE_GCN
#if WAVE_INTRINSICS_ENABLED
float WaveLaneBroadcast(float x, uint lane_index)
{
    return WaveReadLaneAt(x, lane_index);
}

float WaveLaneRotate(float x, int rotate, uint rotate_fix_mask = -1)
{
    uint lane_index = WaveGetLaneIndex();
    uint lane_count = WaveGetLaneCount();
    rotate_fix_mask = rotate_fix_mask == -1 ? ~(WaveGetLaneCount() - 1) : rotate_fix_mask;
    uint src_lane_index = (rotate_fix_mask & lane_index) | ((~rotate_fix_mask) & uint(lane_index + rotate));
    src_lane_index = (src_lane_index + lane_count) % lane_count;
    return WaveReadLaneAt(x, src_lane_index);
}

float WaveLaneQuadSwap(float x)
{
    uint src_lane_index = WaveGetLaneIndex() ^ 0x1;
    return WaveReadLaneAt(x, src_lane_index);
}

//swap pair of lanes
float WaveButterflySwap(float x, int offset)
{
    uint src_lane_index = WaveGetLaneIndex() ^ offset;
    return WaveReadLaneAt(x, src_lane_index);
}

//simulate the ds_swizzle_b32 instruction
float WaveLaneSwizzleGCN(float x, const uint and_mask, const uint or_mask, const uint xor_mask)
{
    uint lane_index = WaveGetLaneIndex();
    uint lane_count = WaveGetLaneCount();
    return WaveReadLaneAt(x, (((lane_index & and_mask) | or_mask) ^ xor_mask) % lane_count); //((L & AND_MASK) | OR_MASK) ^ XOR_MASK
}
#endif
#else
#error "only support platform with wave intrinsics, you can use LDS/sync to realize it"
#endif

#if QUAD_INTRINSICS_ENABLED
//https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_7_QuadAny_QuadAll.html
bool QuadActiveAnyTrue(bool expr)
{
    const uint uint_expr = (uint) expr;
    uint result = uint_expr;
    result |= QuadReadAcrossX(uint_expr);
    result |= QuadReadAcrossY(uint_expr);
    result |= QuadReadAcrossDiagonal(uint_expr);
    return result != 0u;
}

bool QuadActiveAllTrue(bool expr)
{
    return expr && QuadReadAcrossX(expr) && QuadReadAcrossY(expr) && QuadReadAcrossDiagonal(expr);
}
#else
#endif

#if HITILE_LOOKUP_INTRINSICS_ENABLED
#else
#endif

//to do platform related
/**
* \brief swizzled (morton Order) index calculation, location (byte address and tile bit shift offset) of the cmask
* \param tile_coord: cmask tile coordinate
* \param pool_index: cmask morton index (cmask//8 is the byte address)
* \param limit_size: cmask offset shift in byte
*/
void CalculateCmaskIndexAndShift(uint2 tile_coord, out uint cmask_index, out uint cmask_shift)
{
    
}

#endif 