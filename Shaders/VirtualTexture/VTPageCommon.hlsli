#ifndef _VT_PAGE_COMMON_INC_
#define _VT_PAGE_COMMON_INC_

//single page size 
#define VT_LOG2_PAGE_SIZE 7u //128x128
#define VT_PAGE_SIZE (1u << VT_LOG2_PAGE_SIZE)
#define VT_PAGE_SIZE_MASK = (VT_PAGE_SIZE - 1u)

//virtual page table dimensions (128 x 128) 
#define VT_LOG2_PAGE_LEVEL0_POOL_DIMS 7u  //for total 16K phyx page 
#define VT_PAGE_LEVEL0_POOL_DIMS (1u << VT_LOG2_PAGE_LEVEL0_POOL_DIMS)
#define VT_PAGE_POOL_LEVELS (VT_LOG2_PAGE_POOL_DIMS + 1u)

#define VT_VIRTUAL_RESOLUTION_SIZE (VT_PAGE_LEVEL0_POOL_DIMS * VT_PAGE_SIZE)

//page table layout in 2D texture atlas( level0, then remain mip levels next to it in axis-y)  
#define VT_PAGE_TABLE_TEXTURE2D_SIZEX VT_PAGE_LEVEL0_POOL_DIMS
#define VT_PAGE_TABLE_TEXTURE2D_SIZEY (VT_PAGE_LEVEL0_POOL_DIMS + VT_PAGE_LEVEL0_POOL_DIMS / 2u)

struct VTPhysXPageMetaData
{
    uint flags;
    uint lru_frame_index;
    uint mip_level;
    uint owner_id; //virtual texture owner
};

#define VT_PHYSX_RESERVED_FLAGS_BITS 12u
#define VT_PHYSX_RESERVED_FLAGS_SHIFT 20u

struct VTPhysXPage
{
    uint flags; //reserved flags
    uint2 phyx_address;
};

//virtual page texel offset in global page table
struct VTVirtualPageOffset
{
    uint2 texel_offset;
};

inline uint PackVTPhysXPage(in VTPhysXPage physx_page)
{
    uint packed = uint(0u);
    packed |= (physx_page.phyx_address.x & 0x3ffu) | ((physx_page.phyx_address.y & 0x3ffu) << 10u);
    packed |= (physx_page.flags & 0xfffu) << VT_PHYSX_RESERVED_FLAGS_SHIFT;
    //todo 
    return packed;
}

inline VTPhysXPage UnPackVTPhysXPage(uint packed)
{
    VTPhysXPage result;
    
    result.phyx_address = uint2(packed & 0x3ffu, (packed >> 10u) & 0x3ffu);
    result.flags = (packed >> VT_PHYSX_RESERVED_FLAGS_SHIFT) & 0xfffu;
    return result;
}

inline uint PackVTVirtualPageOffset(in VTVirtualPageOffset offset)
{
    return (offset.texel_offset.x << 16u) | offset.texel_offset.y;
}

inline void UnPackVTVirtualPageOffset(uint packed, inout VTVirtualPageOffset offset)
{
    offset.texel_offset.x = packed >> 16u;
    offset.texel_offset.y = packed & 0xffffu;
}

inline uint GetLevelLog2Pages(uint level)
{
    return uint(VT_LOG2_PAGE_LEVEL0_POOL_DIMS) - level;
}

inline uint GetLevelPages(uint level)
{
    return 1u << GetLevelLog2Pages(level);
}

inline uint GetLevelTexels(uint level)
{
    return uint(VT_VIRTUAL_RESOLUTION_SIZE) >> level;
}

//level0,then combine level1-leveln
inline uint2 GetLevelOffsets(uint level)
{
    uint2 offset = uint2(0u, 0u);
    if (level)
    {
        offset.y += 
    }
    return offset;
}

inline uint4 GatherPageTable(Texture2D<uint> page_table, uint2 page_offset, float2 uv, float uv_factor)
{
    
}


#endif //_VT_PAGE_COMMON_INC_