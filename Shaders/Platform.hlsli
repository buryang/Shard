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

#define WAVE_INTRINSICS_ENABLED 

#define WorkDistributionQueue RWByteAddressBuffer
#define WorkDistributionStateQueue RWByteAddressBuffer
#define RWCoherentByteAddressBuffer globallycoherent RWByteAddressBuffer
#define RWCoherentStructuredBuffer(structure) globallycoherent RWStructuredBuffer<structure>

//simulate some ds_swizzle_b32 operations
#ifndef COMPILER_WITH_SWIZZLE_GCN
#ifdef WAVE_INTRINSICS_ENABLED
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