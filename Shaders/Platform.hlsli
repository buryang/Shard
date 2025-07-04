#ifndef _PLATFORM_INC_
#define _PLATFOTM_INC_

//todo platform defines
#define SM6_PROFILE 1
#ifndef SM5_PROFILE
#define SM5_PROFILE 0
#endif

#ifdef __spirv__ //for DXC compile to spir-v
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

//Pre-SM5 GPUs (e.g., DirectX 10/11-era hardware) have stricter constraints on register usage and 
//control flow. Forcing [unroll] could lead to register spills or bloated code, harming performance
#if SM6_PROFILE || SM5_PROFILE || COMPILER_SUPPORTS_ATTRIBUTES
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
#define FASTOPT [fastopt]

//whether platform has wave intrinsics
#define WAVE_INTRINSICS_ENABLED  1
#define PACKING_INTRINSICS_ENABLED 1
#define QUAD_INTRINSICS_ENABLED 1
#define BITFIELD_INTRINSICS_ENABLED 0
//whether platform support hitile lookup
#define HITILE_LOOKUP_INTRINSICS_ENABLED    1

#define COMPILER_SUPPORT_MINMAX3 0

#if 1//AMD 
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

/*
*consecutive threads in a warp access(arrange as warp order) ​​strided memory addresses​​, 
*reducing memory coalescing efficiency.(e.g., Thread 0 → addr[0], Thread 1 → addr[8], 
*Thread 2 → addr[16], ...) while in unwarped(arrange in lane order) layout consecutive 
*threads access ​​contiguous memory addresses​​: (e.g., Thread 0 (lane0/warp0) → addr[0], 
*Thread 1 (lane0/warp1) → addr[1], Thread 2 (lane0/warp2) → addr[2], ...)
*using unwarped index to ​​optimize global memory access/avoid bank conflict/data reshape
*/

uint GetUnwarpedThreadIndex(uint thread_index, uint thread_group_size, uint warp_size = 32)
{
    uint warp_num = (thread_group_size + warp_size - 1) / warp_size;
    uint lane_id = thread_index % warp_size;
    uint warp_id = thread_index / warp_size;
    return lane_id * warp_num + warp_id;
}

/*
*bank conflicts are avoidable in most CUDA computations if care is taken when accessing __shared__
*memory arrays. We can avoid most bank conflicts in scan by adding a variable amount of padding to 
*each shared memory array index we compute. Specifically, we add to the index the value of the index 
*divided by the number of shared memory banks in CUDA bank-conflict free solve as:
#define NUM_BANKS 16
#define LOG_NUM_BANKS 4
#define CONFLICT_FREE_OFFSET(n)((n) >> NUM_BANKS + (n) >> (2 * LOG_NUM_BANKS))
*/
#if 0
uint GetWarpedThreadIndex(uint unwarped_thread_index, uint thread_group_size, uint warp_size = 32)
{
    uint warp_num = (thread_group_size + warp_size - 1) / warp_size;
    uint lane_id = unwarped_thread_index / warp_num;
    uint warp_id = unwarped_thread_index % warp_num;
    return warp_id * warp_size + lane_id;
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