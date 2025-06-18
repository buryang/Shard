#ifndef _COMPUTE_SHADER_UTILS_INC_
#define _COMPUTE_SHADER_UTILS_INC_

#include "CommonUtils.hlsli"
#include "ShaderAssert.hlsli"

/******************************************************************************
\brief help function for compute shaders, simulation cmask/tile operation etc
*******************************************************************************/

uint2 ThreadIndexToPixel(uint element) //element [0, 63] for tile size 8x8
{
#ifndef USE_MORTON_CODE
    uint2 pixel_pos;
    pixel_pos.x = ((element >> 0u) & 1u) | ((element >> 1u) & 2u) | ((element >> 2u) & 4u);
    pixel_pos.y = ((element >> 1u) & 1u) | ((element >> 2u) & 2u) | ((element >> 4u) & 4u);
    return pixel_pos;
#else
    return uint2(ReverseMorton2D(element & 0xffu));
#endif
}
uint2 SwizzleThreadIndex(uint thread_index)
{
#ifdef USE_LOOKUP_TBL
    const uint2 lookup_tbl[64] =
    {
        ThreadIndexToPixel(8 * 0 + 0),
		ThreadIndexToPixel(8 * 0 + 1),
		ThreadIndexToPixel(8 * 0 + 2),
		ThreadIndexToPixel(8 * 0 + 3),
		ThreadIndexToPixel(8 * 0 + 4),
		ThreadIndexToPixel(8 * 0 + 5),
		ThreadIndexToPixel(8 * 0 + 6),
		ThreadIndexToPixel(8 * 0 + 7),
		ThreadIndexToPixel(8 * 1 + 0),
		ThreadIndexToPixel(8 * 1 + 1),
		ThreadIndexToPixel(8 * 1 + 2),
		ThreadIndexToPixel(8 * 1 + 3),
		ThreadIndexToPixel(8 * 1 + 4),
		ThreadIndexToPixel(8 * 1 + 5),
		ThreadIndexToPixel(8 * 1 + 6),
		ThreadIndexToPixel(8 * 1 + 7),
		ThreadIndexToPixel(8 * 2 + 0),
		ThreadIndexToPixel(8 * 2 + 1),
		ThreadIndexToPixel(8 * 2 + 2),
		ThreadIndexToPixel(8 * 2 + 3),
		ThreadIndexToPixel(8 * 2 + 4),
		ThreadIndexToPixel(8 * 2 + 5),
		ThreadIndexToPixel(8 * 2 + 6),
		ThreadIndexToPixel(8 * 2 + 7),
		ThreadIndexToPixel(8 * 3 + 0),
		ThreadIndexToPixel(8 * 3 + 1),
		ThreadIndexToPixel(8 * 3 + 2),
		ThreadIndexToPixel(8 * 3 + 3),
		ThreadIndexToPixel(8 * 3 + 4),
		ThreadIndexToPixel(8 * 3 + 5),
		ThreadIndexToPixel(8 * 3 + 6),
		ThreadIndexToPixel(8 * 3 + 7),
		ThreadIndexToPixel(8 * 4 + 0),
		ThreadIndexToPixel(8 * 4 + 1),
		ThreadIndexToPixel(8 * 4 + 2),
		ThreadIndexToPixel(8 * 4 + 3),
		ThreadIndexToPixel(8 * 4 + 4),
		ThreadIndexToPixel(8 * 4 + 5),
		ThreadIndexToPixel(8 * 4 + 6),
		ThreadIndexToPixel(8 * 4 + 7),
		ThreadIndexToPixel(8 * 5 + 0),
		ThreadIndexToPixel(8 * 5 + 1),
		ThreadIndexToPixel(8 * 5 + 2),
		ThreadIndexToPixel(8 * 5 + 3),
		ThreadIndexToPixel(8 * 5 + 4),
		ThreadIndexToPixel(8 * 5 + 5),
		ThreadIndexToPixel(8 * 5 + 6),
		ThreadIndexToPixel(8 * 5 + 7),
		ThreadIndexToPixel(8 * 6 + 0),
		ThreadIndexToPixel(8 * 6 + 1),
		ThreadIndexToPixel(8 * 6 + 2),
		ThreadIndexToPixel(8 * 6 + 3),
		ThreadIndexToPixel(8 * 6 + 4),
		ThreadIndexToPixel(8 * 6 + 5),
		ThreadIndexToPixel(8 * 6 + 6),
		ThreadIndexToPixel(8 * 6 + 7),
		ThreadIndexToPixel(8 * 7 + 0),
		ThreadIndexToPixel(8 * 7 + 1),
		ThreadIndexToPixel(8 * 7 + 2),
		ThreadIndexToPixel(8 * 7 + 3),
		ThreadIndexToPixel(8 * 7 + 4),
		ThreadIndexToPixel(8 * 7 + 5),
		ThreadIndexToPixel(8 * 7 + 6),
		ThreadIndexToPixel(8 * 7 + 7),
    };
    return lookup_tbl[thread_index];
#else
    return ThreadIndexToPixel(thread_index);
#endif 
}

//[numthread(8,8,1)]
void FastClear(uint2 groupID : SV_GroupID, uint thread_index : SV_GroupIndex, uint4 view_rect, out RWByteAddressBuffer out_cmask, out Texture2D<uint> out_visualized)
{
    const uint2 pixel_local_pos = SwizzleThreadIndex(thread_index & 0xffu);
    const uint2 pixel_pos = ((groupID << 3) | pixel_local_pos) + view_rect.xy;
    
    if (any(pixel_pos.xy) >= view_rect.zw)
        return;
    
    //compute memory location(byte address and tile bit shift offset) of cmask data 
    uint index, shift;
    const uint2 tile_coord = pixel_pos / 8u;
    const uint sub_tile_index = ((pixel_pos.y & 4) >> 1) + ((pixel_pos.x & 4) >> 2);
    //todo
}

/************************************************************************************************************/
/* htile operations, min-max depth pack/unpack, for computer shader cannot use ROP, so define self contain  */
/* HTILE op functions ?                                                                                     */
/************************************************************************************************************/
//no hi-stencil, only min and max depth

#define MAX_DEPTH_RANGE float((1 << 14)-1)

uint PackTileMinMaxDepth(float min_depth, float max_depth)
{
#if 0
#else
    const uint zmin = uint(floor(min_depth * MAX_DEPTH_RANGE));
    const uint zmax = uint(ceil(max_depth * MAX_DEPTH_RANGE));
    const uint htile_val = (zmin << 18) | (zmax << 4);
#endif
    return 0u; //todo
}

uint PackZRangeDelta(uint zbase, uint zother)
{
    uint delta = abs(int(zbase) - int(zother));
    if(delta < 16u)
        return delta;
    const uint leading_zeros = (31u - firstbithigh(delta)) - 18u;
    if(leading_zeros >= 8u)
    {
        return ((11u - leading_zeros) << 3u) | ((delta >> (10u - leading_zeros)) & 0x7u);
    }
    return ((15u - leading_zeros) << 2u) | ((delta >> (11u - leading_zeros)) & 0x3u);
}

//withhi-stencil, packzbase,zdeta, smem, sr
uint PackTileZBaseDeltaWithStencil(float min_depth, float max_depth, uint xx, uint smem, uint sr0, uint sr1, bool is_compareGE)
{
    const uint zmask = 0xf; //expanded
    const uint zmin = uint(floor(min_depth * MAX_DEPTH_RANGE));
    const uint zmax = uint(ceil(max_depth * MAX_DEPTH_RANGE));
    
    //assumes reverse z ??
    const uint zbase = is_compareGE ? zmax : zmin;
    const uint zdelta = PackZRangeDelta(zmin, zmax);
    
    uint htile_val = zmask; // 0:3
    htile_val |= (sr0 << 4); //  4:5
    htile_val |= (sr1 << 6); //  6:7
    htile_val |= (smem << 8); //  8:9
    htile_val |= (xx << 10); // 10:11
    htile_val |= (zdelta << 12); // 12:17
    htile_val |= (zbase << 18); // 18:31
    return htile_val;
}

//unpack 6-bit depth delta
uint UnpackDepthDelta(uint zdelta)
{
    const bool is_two_bit_delta = UnpackBits(zdelta, 5, 1);
    const uint delta_bits = is_two_bit_delta ? 2 : 3;
    const uint delta = UnpackBits(zdelta, 0, delta_bits);
    const uint code = UnpackBits(zdelta, delta_bits, 3);
    
    const uint leading_one = code + (is_two_bit_delta ? 6 : 2);
    const uint ones = (1u << (leading_one + 1)) - 1;
    const uint delta_start = (leading_one >= delta_bits) ? (leading_one - delta_bits) : 0;
    const uint mask = BitsMask(delta_bits, delta_start);

    return InsertBits(mask, delta << delta_start, ones);
}

//unpack min-max format tile data
uint2 UnpackTileMinMax(uint htile_val, bool with_stencil, bool is_compareGE)
{
    uint zmin, zmax;
    if (with_stencil)
    {
        const uint zbase = UnpackBits(htile_val, 18, 14); //18:31
        const uint zdelta = UnpackBits(htile_val, 12, 6); //12:17
        
        zmin = is_compareGE ? (zbase - UnpackDepthDelta(zdelta)) : zbase;
        zmax = is_compareGE ? zbase : (zbase + UnpackDepthDelta(zdelta));
    }
    else
    {
        zmin = UnpackBits(htile_val, 4, 14);
        zmax = UnpackBits(htile_val, 18, 14);
    }
    return uint2(zmin, zmax);
}

uint2 UnpackTileStencil(uint htile_val, bool with_stencil)
{
    uint sr0 = 0u, sr1 = 0u;
    if (with_stencil)
    {
        sr0 = UnpackBits(htile_val, 4, 2);
        sr1 = UnpackBits(htile_val, 6, 2);
    }
    return uint2(sr0, sr1);
}

uint ZMaskToZPlaneCount(uint zmask)
{
    uint plane_count = zmask;
    if(plane_count >= 11u)
    {
        plane_count += 2u;
    }
    else if(plane_count >= 9u)
    {
        ++plane_count;
    }
    return plane_count;
}

//calculate cmask data's memory location (byte address and tile bit shift offset) 
void CalculateCmaskIndexAndShift(uint2 tile_coord, out uint cmask_index, out uint cmask_shift)
{
    
}


//memory copy utils
void MemoryCopy(ByteAddressBuffer src, uint64 src_offset, out RWByteAddressBuffer dst, uint64 dst_offset, uint64 size, uint3 thread_dispatchID : SV_DispatchThreadID) //uint3??
{
    //assume dst_offset£¬src_offset, size align to uint4
    uint dword_offset = thread_dispatchID.x << 4; 
    if (dword_offset < size)
    {
        const uint4 data = src.Load4(src_offset + dword_offset);
        dst.Store4(dst_offset + dword_offset, data);
    }
}
#endif //_COMPUTE_SHADER_UTILS_INC_