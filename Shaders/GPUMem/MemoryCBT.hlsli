#ifndef _MEMORY_CBT_INC_
#define _MEMORY_CBT_INC_

#include "../BindlessResource.hlsli"

/**
\brief help function for CBT memory manager
*/

#define BUILD_1M
#define NATIVE_DECODE

//the maimal size of the lDS is 16k bytes
#ifndef WORKGROUP_SIZE
#define WORKGROUP_SIZE 64
#endif

/*
Level 0: 32 bit // [0, 524288] x 1, needs a minimum of 20 bits (rounded up to 32 for alignment and required for atomic operations)
Level 1: 32 bit // [0, 262144] x 2, needs a minimum of 19 bits (rounded up to 32 for alignment and required for atomic operations)
Level 2: 32 bit // [0, 131072] x 4, needs a minimum of 18 bits (rounded up to 32 for alignment and required for atomic operations)
...
Level 6: 32 bit // [0, 8192] x 64, needs a minimum of 14 bits (rounded up to 16 for alignment and bumped to 32 bits for atomic operations)
------------------------------------------------------------------------------------------------------------------------------------------
Level 7: 16 bit // [0, 4096] x 128, needs a minimum of 13 bits (rounded up to 16 for alignment and bumped to 32 bits for atomic operations)
Level 8: 16 bit // [0, 2048] x 256, needs a minimum of 12 bits (rounded up to 16 for alignment)
....
------------------------------------------------------------------------------------------------------------------------------------------
Level last-1[n]: 8 bit // [0, 128] x 2^n, needs a mininum of 8bits

Level last: Raw 64bits representation
***use low bits in high level to save group memory
*/

#ifdef BUILD_128K
#define TOTAL_ELEMENT_CAPACITY 131072
#define CBT_TREE_SIZE (32*(1+2+4+8+16+32+64) + 16*(128+256+512) + 8*1024)
#define CBT_LAST_LEVEL_SIZE 1024
#define TREE_LAST_LEVEL 10
#define LEAF_LEVEL 17
#elif defined(BUILD_512K)
#define TOTAL_ELEMENT_CAPACITY 524288
#define CBT_TREE_SIZE (32*(1+2+4+8+16+32+64) + 16*(128+256+512+1024+2048) + 8*4096)
#define CBT_LAST_LEVEL_SIZE 4096
#define TREE_LAST_LEVEL 12
#define LEAF_LEVEL 19
#elif defined(BUILD_1M)
#define TOTAL_ELEMENT_CAPACITY 1048576
#define CBT_TREE_SIZE (32*(1+2+4+8+16+32+64) + 16*(128+256+512+1024+2048+4096) + 8*8192)
#define CBT_LAST_LEVEL_SIZE 8192
#define TREE_LAST_LEVEL 13
#define LEAF_LEVEL 20
#else
#error "only support 128k, 512k, 1M as cbt orignal github code"
#endif 

#define CBT_TREE_NUM_SLOTS (CBT_TREE_SIZE_BITS / 32)
#define CBT_BITFIELD_NUM_SLOTS (TOTAL_ELEMENT_CAPACITY / 64)
#define FIRST_VIRTUAL_LEVEL (TREE_LAST_LEVEL+1)

#define BUFFER_ELEMENT_PER_LANE ((CBT_TREE_NUM_SLOTS+WORKGROUP_SIZE-1)/WORKGROUP_SIZE)
#define BUFFER_ELEMENT_PER_LANE_NO_BITFIELD ((CBT_TREE_NUM_SLOTS + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE)
#define BITFIELD_ELEMENT_PER_LANE ((CBT_BITFIELD_NUM_SLOTS + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE)
#define WAVE_TREE_DEPTH uint(firstbithigh(TOTAL_ELEMENT_CAPACITY))

#define CBT_ARRAY_SLOTS (LEAF_LEVEL+1)
//per level _offset
static const uint cbt_depth_offset[CBT_ARRAY_SLOTS] =
{
    0, // Level 0
    32 * 1, // level 1
    32 * 1 + 32 * 2, // level 2
    32 * 1 + 32 * 2 + 32 * 4, // level 3
    32 * 1 + 32 * 2 + 32 * 4 + 32 * 8, // Level 4
    32 * 1 + 32 * 2 + 32 * 4 + 32 * 8 + 32 * 16, // Level 5
    32 * 1 + 32 * 2 + 32 * 4 + 32 * 8 + 32 * 16 + 32 * 32, // Level 6
    32 * 1 + 32 * 2 + 32 * 4 + 32 * 8 + 32 * 16 + 32 * 32 + 32 * 64, // Level 7

    32 * 1 + 32 * 2 + 32 * 4 + 32 * 8 + 32 * 16 + 32 * 32 + 32 * 64 + 16 * 128, // Level 8
    32 * 1 + 32 * 2 + 32 * 4 + 32 * 8 + 32 * 16 + 32 * 32 + 32 * 64 + 16 * 128 + 16 * 256, // Level 9
    32 * 1 + 32 * 2 + 32 * 4 + 32 * 8 + 32 * 16 + 32 * 32 + 32 * 64 + 16 * 128 + 16 * 256 + 16 * 512, // Level 10 
#ifndef BUILD_128L
    32 * 1 + 32 * 2 + 32 * 4 + 32 * 8 + 32 * 16 + 32 * 32 + 32 * 64 + 16 * 128 + 16 * 256 + 16 * 512 + 16 * 1024, // Level 11
    32 * 1 + 32 * 2 + 32 * 4 + 32 * 8 + 32 * 16 + 32 * 32 + 32 * 64 + 16 * 128 + 16 * 256 + 16 * 512 + 16 * 1024 + 16 * 2048, // Level 12
#ifdef BUILD_1M
    32 * 1 + 32 * 2 + 32 * 4 + 32 * 8 + 32 * 16 + 32 * 32 + 32 * 64 + 16 * 128 + 16 * 256 + 16 * 512 + 16 * 1024 + 16 * 2048 + 16 * 4096, // Level 13
#endif 
#endif
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static uint64 cbt_bit_mask[CBT_ARRAY_SLOTS] =
{
    0xffffffff, // Root 17
    0xffffffff, // Level 16
    0xffffffff, // level 15
    0xffffffff, // level 14
    0xffffffff, // level 13
    0xffffffff, // level 12
    0xffffffff, // level 11

    0xffff, // level 10
    0xffff, // level 9
#ifndef BUILD_128K
    0xffff, // level 8
    0xffff, // level 8
#ifndef BUILD_512K
    0xffff, // level 8
#endif
#endif
    0xffff, // level 8
    0xff, // level 8

    0xffffffffffffffff, // level 7
    0xffffffff, // Level 6
    0xffff, // level 5
    0xff, // level 4
    0xf, // level 3
    0x3, // level 2
    0x1, // level 1
};

static uint cbt_bit_count[CBT_ARRAY_SLOTS] =
{
    32, // Root 17
    32, // Level 16
    32, // level 15
    32, // level 14
    32, // level 13
    32, // level 12
    32, // level 11

    16, // level 10
    16, // level 9
#ifndef BUILD_128K
    16, // level 8
    16, // level 8
#ifndef BUILD_512K
    16, // level 8
#endif
#endif
    16, // level 8
    8, // level 8

    64, // Level 5
    32, // Level 5
    16, // Level 4
    8, // level 3
    4, // level 2
    2, // level 1
    1, // level 0
};

struct CBTConfig
{
    uint element_size;
    //uint cbt_depths;
    uint cbt_buffer_rw_index; //heap buffer index in bindless, uint32 
    uint bits_field_rw_index; //used bits buffer index in bindless, uint64 bits
    uint allocation_buffer_rw_index; //uint32
};

/************************************************************************************/
/*                             MEMORY LAYOUT                                        */
/*         HEAD         |                HEAP                                       */
/*      +CBT+BITFIELD+  | ELEMENT+ELEMENT+ELEMT+...+ELEMENT                         */
/************************************************************************************/

RWBuffer<uint> heap; //temporal struture 

cbuffer CBTConfigBuffer
{
    CBTConfig cbt_cfg;
};

//Atomic-friendly Sub-tree (uint32_t aligned, group shared memory)
//Thread Sub-trees (Min size, aligned on powers of 2, group shared memory)
groupshared uint gs_cbt_tree[CBT_BITFIELD_NUM_SLOTS];

inline uint CBTSize()
{
    return TOTAL_ELEMENT_CAPACITY;
}

inline uint LastLevel_offset()
{
    return cbt_depth__offset[TREE_LAST_LEVEL] / 32;
}

//load and export, load cbt to group shared memory
//// Importante note
//Depending on your target GPU architecture, the pattern used to load has a different performance behavior
//here is the best performant based on our tests:
//NVIDIA uint target_element = groupIndex + WORKGROUP_SIZE * e;
//AMD uint target_element = BUFFER_ELEMENT_PER_LANE * groupIndex + e;
void LoadBufferToSharedMemory(uint group_index)
{
    //load the bitfield to the LDS
    for (uint e = 0u; e < BUFFER_ELEMENT_PER_LANE; ++e)
    {
        uint target_element = BUFFER_ELEMENT_PER_LANE * group_index + e;
        gs_cbt_tree[target_element] = GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[target_element];
    }
    GroupMemoryBarrierWithGroupSync();
}

void LoadSharedMemoryToBuffer(uint group_index)
{
    GroupMemoryBarrierWithGroupSync();
    for (uint e = 0u; e < BUFFER_ELEMENT_PER_LANE; ++e)
    {
        uint target_element = BUFFER_ELEMENT_PER_LANE * group_index + e;
        GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[target_element] = gs_cbt_tree[target_element];
    }
}

//functons to set/get bits in gs cbt_tree
void SetBit(uint bitID, bool state)
{
    uint slot = bitID / 64;
    uint local_id = bitID & 0x3F;
    
    if(state)
    {
        GetBindlessRWByteBufferUniform(cbt_cfg.bits_field_rw_index)[slot] |= 1ull << local_id;
    }
    else
    {
        GetBindlessRWByteBufferUniform(cbt_cfg.bits_field_rw_index)[slot] &= ~(1ull << local_id);
    }
    
}

uint GetBit(uint bitID)
{
    uint slot = bitID / 64;
    uint local_id = bitID & 0x3F;
    return uint((GetBindlessRWByteBufferUniform(cbt_cfg.bits_field_rw_index)[slot] >> local_id) & 0x1);
}

//this function do the real allocation work in positon
//find by DecodeBit
void SetBitAtomic(uint bitID, bool state)
{
    uint slot = bitID / 64;
    uint local_id = bitID & 0x3F;
    
    if (state)
    {
        InterlockedOr(GetBindlessRWByteBufferUniform(cbt_cfg.bits_field_rw_index)[slot]£¬1ull << local_id);
    }
    else
    {
        InterlockedAnd(GetBindlessRWByteBufferUniform(cbt_cfg.bits_field_rw_index)[slot], ~(1ull << local_id));
    }
}

//function set bit atomic in memory cbt_tree
void SetBitAtomicBuffer(uint bitID, bool state)
{
    uint slot = bitID / 64;
    uint local_id = bitID & 0x3f;
    if(state)
    {
        InterlockedXor(GetBindlessRWByteBufferUniform(cbt_cfg.bits_field_rw_index)[slot], 1ull << local_id));
    }
    else
    {
        InterlockedAnd(GetBindlessRWByteBufferUniform(cbt_cfg.bits_field_rw_index)[slot], ~(1ull << local_id));
    }
}

//set cbt heap value
void SetHeapElement(uint id, uint value) 
{
    //Figure out the location of the first bit of this element
    uint real_heap_id = id - 1;
    uint depth = uint(log2(real_heap_id + 1));
    uint level_first_element = (1u << depth) - 1;
    uint first_bit = cbt_depth_offset[depth] + cbt_bit_count[depth] * (real_heap_id - level_first_element);

    //Find the slot and the local first bit
    uint slot = first_bit / 32;
    uint local_id = first_bit % 32;

    //Extract the relevant bits
    gs_cbt_tree[slot] &= ~(uint(cbt_bit_mask[depth]) << local_id);
    gs_cbt_tree[slot] |= ((uint(cbt_bit_mask[depth]) & value) << local_id);
}

void SetHeapElementAtomic(uint id, uint value)
{
    // Figure out the location of the first bit of this element
    uint real_heap_id = id - 1;
    uint depth = uint(log2(real_heap_id + 1));
    uint level_first_element = (1u << depth) - 1;
    uint first_bit = cbt_depth_offset[depth] + cbt_bit_count[depth] * (real_heap_id - level_first_element);

    // Find the slot and the local first bit
    uint slot = first_bit / 32;
    uint local_id = first_bit % 32;

    // Extract the relevant bits
    InterlockedAnd(gs_cbt_tree[slot], ~(uint(cbt_bit_mask[depth]) << local_id));
    InterlockedOr(gs_cbt_tree[slot], ((uint(cbt_bit_mask[depth]) & value) << local_id));
}

uint GetHeapElement(uint id)
{
    // Figure out the location of the first bit of this element
    uint real_heap_id = id - 1;
    uint depth = uint(log2(real_heap_id + 1));
    uint level_first_element = (1u << depth) - 1;
    uint id_in_level = real_heap_id - level_first_element;
    uint first_bit = cbt_depth_offset[depth] + cbt_depth_offset[depth] * id_in_level;
    if (depth < FIRST_VIRTUAL_LEVEL)
    {
        uint slot = first_bit / 32;
        uint local_id = first_bit % 32;
        uint target_bits = (gs_cbt_tree[slot] >> local_id) & uint(cbt_bit_mask[depth]);
        return (gs_cbt_tree[slot] >> local_id) & uint32_t(cbt_bit_mask[depth]);
    }
    else
    {
        uint slot = first_bit / 64;
        uint local_id = first_bit % 64;
        uint target_bits = (GetBindlessRWByteBufferUniform(cbt_cfg.bits_field_rw_index)[slot] >> local_id) & cbt_bit_mask[depth];
        return countbits(target_bits);
    }
}

//find the position of the i-th one in the bitfield
//"decode the location of the bits we've allocated"
//in the paper algorithm 7
uint DecodeBit(uint index)
{
#if 0 //def NATIVE_DECODE
    uint bitID = 1u;
    for (uint curr_depth = 0u; curr_depth < xx; ++curr_depth)
    {
        uint heap_value = GetHeapElement(2 * bitID);
        uint b = index < heap_value ? 0 : 1;
        bitID = 2 * bitID + b;
        index -= heap_value * b;
    }
    return (bitID ^ TOTAL_ELEMENT_CAPACITY);
#else
    uint curr_depth = 0u;
    uint heap_elementID = 1u;
    //NATIVE DECODE part
    for(;curr_depth < FIRST_VIRTUAL_LEVEL; ++curr_depth)
    {
        uint heap_value = GetHeapElement(2u*heap_elementID);
        uint b = index < heap_value ? 0u : 1u;
        heap_elementID = 2u * heap_elementID + b;
        index -= heap_value * b;
    }
    ++curr_depth;
    uint64 heap_value = GetBindlessRWByteBufferUniform(cbt_cfg.bits_field_rw_index)[heap_elementID - CBT_LAST_LEVEL_SIZE * 2];
    uint64 mask = 0xffffffff;
    uint bit_count = 32u;
    for (; curr_depth < (WAVE_TREE_DEPTH + 1); ++curr_depth)
    {
        uint real_heap_id = 2 * heap_elementID - 1;
        uint level_first_element = (1u << curr_depth) - 1u;
        uint id_in_level = real_heap_id - level_first_element;
        uint first_bit = bit_count * id_in_level;
        uint local_id = first_bit & 0x3f;
        uint64 target_bits = (heap_value >> local_id) & mask;
        uint heap_value = countbits(target_bits);
        
        //check in the left/right subtree
        uint b = index < heap_value ? 0u : 1u;
        heap_elementID = 2 * heap_elementID + b;
        index -= heap_value * b;
        bit_count /= 2;
        mask = mask >> bit_count;
    }
    return (heap_elementID ^ TOTAL_ELEMENT_CAPACITY);
#endif
}

//find i-th bit set to zero
//in the paper algorithm 8
uint DecodeBitComplement(uint index)
{
#if 0//def NATIVE_DECODE
    uint bitID = 1u;
    uint c = TOTAL_ELEMENT_CAPACITY / 2u;
    while (bitID < TOTAL_ELEMENT_CAPACITY)
    {
        uint heap_value = c - GetHeapElement(2u * bitID);
        uint b = index < heap_value ? 0u : 1u;
        bitID = 2u * bitID + b;
        index -= index * b;
        c >>= 1u;
    }
    return (bitID ^ TOTAL_ELEMENT_CAPACITY);
#else
    uint curr_depth = 0u;
    uint heap_elementID = 1u;
    uint c = TOTAL_ELEMENT_CAPACITY / 2u;
    
    //NATIVE DECODE part
    for(;curr_depth < FIRST_VIRTUAL_LEVEL; ++curr_depth)
    {
        uint heap_value = c - GetHeapElement(2u*heap_elementID);
        uint b = index < heap_value ? 0u : 1u;
        heap_elementID = 2u * heap_elementID + b;
        index -= heap_value * b;
        c >>= 1u;
    }
    ++curr_depth;
    uint64 heap_value = GetBindlessRWByteBufferUniform(cbt_cfg.bits_field_rw_index)[heap_elementID - CBT_LAST_LEVEL_SIZE * 2];
    uint64 mask = 0xffffffff;
    uint bit_count = 32u;
    for (; curr_depth < (WAVE_TREE_DEPTH + 1); ++curr_depth)
    {
        uint real_heap_id = 2 * heap_elementID - 1;
        uint level_first_element = (1u << curr_depth) - 1u;
        uint id_in_level = real_heap_id - level_first_element;
        uint first_bit = bit_count * id_in_level;
        uint local_id = first_bit & 0x3f;
        uint64 target_bits = (heap_value >> local_id) & mask;
        uint heap_value = c - countbits(target_bits);
        
        //check in the left/right subtree
        uint b = index < heap_value ? 0u : 1u;
        heap_elementID = 2 * heap_elementID + b;
        index -= heap_value * b;
        c >>= 1u;
        bit_count /= 2;
        mask = mask >> bit_count;
    }
    return (heap_elementID ^ TOTAL_ELEMENT_CAPACITY);
#endif
}

//get bit count from group shared
inline uint BitCount()
{
    return gs_cbt_tree[0];
}

inline uint BitCount(uint depth, uint element)
{
    return GetHeapElement((1u << depth) + element);//b-tree
}

//get bit count from buffer
inline uint BitCountBuffer()
{
    return GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[0];

}

//allocation, it allocate in order
//first do the allocate work,then use decodebit function to get its positon
uint AllocateNextAvailableSlot(uint num_slot = 1)
{
    uint first_bit_index;
    //large_cbt code use buffer with two element to store allocated slots count
    InterlockedAdd(xx, num_slot, first_bit_index);
    if (first_bit_index > TOTAL_ELEMENT_CAPACITY)
    {
        return -1;
    }
    return first_bit_index;
}

//the orignal slowest reduce alg
void Reduce(uint group_index)
{
    //first we do a reduction until each lane has exactly one element to process
    uint initial_pass_size = TOTAL_ELEMENT_CAPACITY / WORKGROUP_SIZE;
    for (uint it = initial_pass_size / 64u, _offset = TOTAL_ELEMENT_CAPACITY / 64u; it > 0u; it >>= 1u, _offset >>= 1u)
    {
        //merge two subtree
        uint min_heapID = _offset + (group_index * it);
        uint max_heapID = _offset + ((group_index + 1) * it);
        for (uint heapID = min_heapID; heapID < max_heapID; ++heapID)
        {
            SetHeapElement(heapID, GetHeapElement(heapID * 2) + GetHeapElement(heapID * 2 + 1));
        }
    }
    GroupMemoryBarrierWithGroupSync();
    for (uint s = WORKGROUP_SIZE / 2u; s > 0u; s >>=1u )
    {
        if(group_index < s)
        {
            uint v = s + group_index;
            SetHeapElement(v, GetHeapElement(v * 2) + GetHeapElement(v * 2 + 1));
        }
        GroupMemoryBarrierWithGroupSync();
    }
}

//do a reduction to move from our bitfield to the first explicitly stored level of the tree
void ReducePrePass(uint threadID)
{
    const uint buffer__offset = LastLevel_offset();
    
    //pack 4 8bit to uint
    uint pack_sum = 0u;
    for (uint pair_index = 0u; pair_index < 4; ++pair_index)
    {
        uint element_count = countbits(GetBindlessRWByteBufferUniform(cbt_cfg.bits_field_rw_index)[threadID*8+2*pair_index]);
        element_count += countbits(GetBindlessRWByteBufferUniform(cbt_cfg.bits_field_rw_index)[threadID * 8 + 2 * pair_index+1]);
        
        pack_sum |= (element_count << (pair_index * 8));
    }

}

//another pass within the pet-thread atomic part of the tree
void ReduceFirstPass(uint thread_index, uint group_index)
{
    //load the lowest(and only the last level) to groupshared
    const uint level0__offset = cbt_depth__offset[TREE_LAST_LEVEL] / 32u;
#ifdef BUILD_128K
    if (group_index % 2 == 0)
        gs_cbt_tree[level0_offset + thread_index / 2] = GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[level0__offset + thread_index / 2];
#elif defined(BUILD_512K)
    for (uint e = 0; e < 2; ++e)
    {
        uint target_element = 2 * thread_index + e;
        gs_cbt_tree[level0_offset + target_element] = GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[level0__offset + target_element];
    }
#elif defined(BUILD_1M)
    for (uint e = 0; e < 4; ++e)
    {
        uint target_element = 4 * thread_index + e;
        gs_cbt_tree[level0_offset + target_element] = GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[level0__offset + target_element];
    }
#endif
    
    GroupMemoryBarrierWithGroupSync();

    // First we do a reduction until each lane has exactly one element to process
    uint initial_pass_size = CBT_LAST_LEVEL_SIZE / 2;
    uint it, offset;
    for (it = initial_pass_size / 512, _offset = initial_pass_size; it > 1; it >>= 1, _offset >>= 1)
    {
        uint min_heapID = offset + (thread_index * it);
        uint max_heapID = offset + ((thread_index + 1) * it);

        for (uint heapID = min_heapID; heapID < max_heapID; ++heapID)
        {
            SetHeapElement(heapID, GetHeapElement(heapID * 2) + GetHeapElement(heapID * 2 + 1));
        }
    }
    
    // Last pass needs to be atomic
    uint heapID = offset + (thread_index * it);
    SetHeapElementAtomic(heapID, GetHeapElement(heapID * 2) + GetHeapElement(heapID * 2 + 1));

    GroupMemoryBarrierWithGroupSync();
    
    //load data from gs to memory
#ifdef BUILD_128K
    const uint level2_offset = cbt_depth_offset[TREE_LAST_LEVEL - 1] / 32;
    if (group_index % 2 == 0)
        GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[level2_offset + thread_index / 2] = gs_cbt_tree[level2_offset + thread_index / 2];

    const uint level3_offset = cbt_depth_offset[TREE_LAST_LEVEL - 2] / 32;
    if (group_index % 4 == 0)
        GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[level3_offset + thread_index / 4] = gs_cbt_tree[level3_offset + thread_index / 4];
#elif defined(BUILD_512K)
    const uint level2_offset = cbt_depth_offset[TREE_LAST_LEVEL - 1] / 32;
    for (uint e = 0; e < 2; ++e)
    {
        uint target_element = 2 * thread_index + e;
        GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[level2_offset + target_element] = gs_cbt_tree[level2_offset + target_element];
    }

    const uint level3_offset = cbt_depth_offset[TREE_LAST_LEVEL - 2] / 32;
    GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[level3_offset + thread_index] = gs_cbt_tree[level3_offset + thread_index];

    const uint level4_offset = cbt_depth_offset[TREE_LAST_LEVEL - 3] / 32;
    if (groupIndex % 2 == 0)
        GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[level4_offset + thread_index / 2] = gs_cbt_tree[level4_offset + thread_index / 2];
#elif defined(BUILD_1M)
    const uint level2_offset = cbt_depth_offset[TREE_LAST_LEVEL - 1] / 32;
    for (uint e = 0; e < 4; ++e)
    {
        uint target_element = 4 * thread_index + e;
        GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[level2_offset + target_element] = gs_cbt_tree[level2_offset + target_element];
    }

    // Load the first reduced level
    const uint level3_offset = cbt_depth_offset[TREE_LAST_LEVEL - 2] / 32;
    for (uint e = 0; e < 2; ++e)
    {
        uint target_element = 2 * thread_index + e;
        GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[level3_offset + target_element] = gs_cbt_tree[level3_offset + target_element];
    }

    const uint level4_offset = cbt_depth_offset[TREE_LAST_LEVEL - 3] / 32;
    GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[level4_offset + thread_index] = gs_cbt_tree[level4_offset + thread_index];

    const uint level5_offset = cbt_depth_offset[TREE_LAST_LEVEL - 4] / 32;
    if (groupIndex % 2 == 0)
        GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[level5_offset + thread_index / 2] = gs_cbt_tree[level5_offset + thread_index / 2];
#else
    #error "now support 128K, 512K, 1M"
#endif 
    
}

//one single workgroup will do the full sum reduction on the atomic friendly tree
void ReduceSecondPass(uint group_index)
{
    //load the lowest level (and only the last level)
    const uint level0_offset = cbt_depth_offset[9] / 32;
    for (uint e = 0; e < 4; ++e)
    {
        uint target_element = 4 * group_index + e;
        gs_cbt_tree[level0_offset + target_element] = GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[level0_offset + target_element];
    }
    GroupMemoryBarrierWithGroupSync();

    // First we do a reduction until each lane has exactly one element to process
    uint initial_pass_size = 256;
    for (uint it = initial_pass_size / 64, offset = initial_pass_size; it > 0; it >>= 1, offset >>= 1)
    {
        uint min_heapID = offset + (group_index * it);
        uint max_heapID = offset + ((group_index + 1) * it);

        for (uint heapID = min_heapID; heapID < max_heapID; ++heapID)
        {
            SetHeapElement(heapID, GetHeapElement(heapID * 2) + GetHeapElement(heapID * 2 + 1));
        }
    }
    GroupMemoryBarrierWithGroupSync();
    
    for (uint s = WORKGROUP_SIZE / 2; s > 0u; s >>= 1)
    {
        if (group_index < s)
        {
            uint v = s + group_index;
            SetHeapElement(v, GetHeapElement(v * 2) + GetHeapElement(v * 2 + 1));
        }
        GroupMemoryBarrierWithGroupSync();
    }
    
    // Make sure all the previous operations are done
    GroupMemoryBarrierWithGroupSync();

    // Load the bitfield to the LDS
    for (uint e = 0; e < 5; ++e)
    {
        uint target_element = 5 * group_index + e;
        if (target_element < 319)
            GetBindlessRWByteBufferUniform(cbt_cfg.cbt_buffer_rw_index)[target_element] = gs_cbt_tree[target_element];
    }
}

//todo ?? reduce without bitfield???
void ReduceNoBitfield(uint group_index)
{
       // First we do a reduction until each lane has exactly one element to process
    uint initial_pass_size = TOTAL_ELEMENT_CAPACITY / WORKGROUP_SIZE;
    for (uint it = initial_pass_size / 128, offset = TOTAL_ELEMENT_CAPACITY / 128; it > 0; it >>= 1, offset >>= 1) //while Reduce() use 64 instead of 128
    
    {
        uint min_heapID = offset + (group_index * it);
        uint max_heapID = offset + ((group_index + 1) * it);

        for (uint heapID = min_heapID; heapID < max_heapID; ++heapID)
        {
            SetHeapElement(heapID, GetHeapElement(heapID * 2) + GetHeapElement(heapID * 2 + 1));
        }
    }
    GroupMemoryBarrierWithGroupSync();

    for (uint s = WORKGROUP_SIZE / 2; s > 0u; s >>= 1)
    {
        if (group_index < s)
        {
            uint v = s + group_index;
            SetHeapElement(v, GetHeapElement(v * 2) + GetHeapElement(v * 2 + 1));
        }
        GroupMemoryBarrierWithGroupSync();
    }
}

void ClearCbt(uint group_index)
{
    //clear gs cbt
    for (uint v = 0; v < BUFFER_ELEMENT_PER_LANE; ++v)
    {
        uint target_element = BUFFER_ELEMENT_PER_LANE * group_index + v;
        if (target_element < TOTAL_ELEMENT_CAPACITY)
            gs_cbt_tree[target_element] = 0u;
    }
    
    //clear bitfield
    for (uint b = 0; b < BITFIELD_ELEMENT_PER_LANE; ++b)
    {
        uint target_element = BITFIELD_ELEMENT_PER_LANE * group_index + b;
        if (target_element < CBT_BITFIELD_NUM_SLOTS)
            GetBindlessRWByteBufferUniform(cbt_cfg.bits_field_rw_index)[target_element] = 0ull;

    }
    GroupMemoryBarrierWithGroupSync();
}

#if 0 //define below function where you need
//reduce pass pre->first->second
/*reduce pre pass*/
[numthreads(WORKGROUP_SIZE, 1, 1)]
void CSMainRPP(uint dispatch_threadID : SV_DispatchThreadID)
{
    ReducePrePass(dispatch_threadID);
}

/*reduce first pass*/
[numthreads(WORKGROUP_SIZE, 1, 1)]
void CSMainRFP(uint dispatch_threadID : SV_DispatchThreadID, uint group_index : SV_GroupIndex)
{
    ReduceFirstPass(dispatch_threadID, group_index);
}

/*reduce second pass*/
void CSMainRSP(uint group_index : SV_GroupIndex)
{
    ReduceSecondPass(group_index);
}
#endif

#endif