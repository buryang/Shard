#ifndef _MEMORY_ARENA_INC_
#define _MEMORY_ARENA_INC_
#include "../CommonUtils.hlsli"
#include "../BindlessResource.hlsli"

struct ArenaConfig
{
    uint max_allocation_size;
    uint granularity_byte_shift;
    
    //bindless buffer index
    uint free_size_ranges_index;
    uint free_gaps_pos_binned_index;
    
    uint arena_base_index;
    uint arena_used_bits_index;
    uint heap_index;
};

cbuffer ConfigBuffer : register(b0, BUFFER_SPACE)
{
    ArenaConfig arena_config;
};

bool FindAllocation(inout uint alloc_size, inout uint alloc_pos, uint attempts, uint request_growth)
{
    const uint max_allocate_size = arena_config.max_allocation_size;
    uint request_size = alloc_size;
    bool found = false;
    
    while(!found && attempts > 0 && request_size <= max_allocate_size)
    {
        int index;
        InterlockedAdd(GetBindlessRWByteBufferUniform(arena_config.free_size_ranges_index)[request_size], -1, index);
        
        if(index >= 1)
        {
            found = true;
            uint range_offset = GetBindlessRWByteBufferUniform(arena_config.free_size_ranges_index).Load(request_size);
            alloc_pos = GetBindlessRWByteBufferUniform(arena_config.free_gaps_pos_binned_index).Load(0); //todo
            
        }
        else
        {
            request_size += request_growth;
            --attempts;
        }
    }
    
    if (found)
    {
        alloc_size = request_size;
    }

    return found;
}

void TagArenaUsedBits(uint alloc_pos, uint alloc_size, bool is_accquire)
{
    uint start_pos = alloc_pos;
    uint end_pos = alloc_pos + alloc_size - 1;
    
    uint start_bit = start_pos & 31;
    uint end_bit = end_pos & 31;
    
    uint start32 = start_pos / 32;
    uint end32 = end_pos / 32;
    
    uint start_mask = ~0u;
    uint end_mask = ~0u;
    
    if (start_bit)
    {
        start_mask = ~((1u << start_bit) - 1);
    }
    
    if(end_bit != 31)
    {
        end_mask = (1u << (end_bit + 1u)) - 1u;
    }
    
    if (is_accquire)
    {
        InterlockedOr(xx[start32], start_mask);
    }
    else
    {
        InterlockedAnd(xx[start32], ~start_mask);
    }
    
    if (start32)
    {
        uint mask = is_accquire ? ~0u : 0u;
        for (uint i = start32 + 1; i < end32; ++i)
        {
            xx[i] = mask; //todo
        }

        if (is_accquire)
        {
            InterlockedOr(xx[end32], end_mask);
        }
        else
        {
            InterlockedAnd(xx[end32], ~end_mask);
        }
    }
    
}

#if 1 //def BUILD_ARENA_FGB

//compute shader entry for free gaps builder
void CSMainFGB()
{
    //different from nvidia code, work done in a wave
    const uint sectorID = 0u;
}

#endif

#if 1 //def BUILD_ARENA_FGI
//compute shader entry for free gaps inserting 
void CSMainFGI(uint3 dispatchID : SV_thread_index)
{
    uint threadID = dispatchID.x;
    bool valid = threadID < xxx;
    
    if(valid)
    {
        uint free_gap_pos = xx;
        uint free_gap_size = yy;
        
        uint range_index;
        InterlockedAdd(zz, 1, range_index);
        uint range_offset = xxx;
        uint store_offset = range_index + uint(range_offset);
        //todo
    }
}
#endif




#endif 