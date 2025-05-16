#include "../CommonUtils.hlsli"
#include "../Platform.hlsli"
#include "../BindlessResource.hlsli"
#include "../ComputeShaderUtils.hlsli"
#include "../ShaderAssert.hlsli"
#include "../Material.hlsli"
#include "RasterCommon.hlsli"

#ifndef BINNING_TECHNIQUE 
#define BINNING_TECHNIQUE 1 //thread group bigger than one wave
#endif

#ifndef OPTIMIZE_WRITE_MASK
#define OPTIMIZE_WRITE_MASK 1
#endif

//#define SHADING_BIN_COUNT
#define SHADING_BIN_RESERVE
//#define SHADING_BIN_SCATTER

#define BINNING_THREADS_PER_SHADING_TILE (8) //todo
#define CMASK_HTILE_TILE_SIZE 8 //(AMD GCN/RDNA )
#define QUAD_PIXEL_NUM 4

#if BINNING_TECHNIQUE == 1
#define SHADING_BIN_TILE_SIZE 32
#else
#define SHADING_BIN_TILE_SIZE 8 //amd 64 wavefront
#endif 

#define SHADING_BIN_TILE_THREADS (SHADING_BIN_TILE_SIZE*SHADING_BIN_TILE_SIZE) 

#ifndef SHADING_VARIABLE_RATE
#define SHADING_VARIABLE_RATE 1
#endif

#if 1
#define SHADING_VRS_2X2 D3D12_SHADING_RATE_2X2
#define SHADING_VRS_2X1 D3D12_SHADING_RATE_2X1
#define SHADING_VRS_1X2 D3D12_SHADING_RATE_1X2
#define SHADING_VRS_1X1 D3D12_SHADING_RATE_1X1
#else
#define SHADING_VRS_2X2 VK_FRAGMENT_SHADING_RATE_1_INVOCATION_PER_2X2_PIXELS_KHR 
#define SHADING_VRS_2X1 VK_FRAGMENT_SHADING_RATE_1_INVOCATION_PER_2X1_PIXELS_KHR   //
#define SHADING_VRS_1X2 VK_FRAGMENT_SHADING_RATE_1_INVOCATION_PER_1X2_PIXELS_KHR   //
#define SHADING_VRS_1X1 VK_FRAGMENT_SHADING_RATE_1_INVOCATION_PER_PIXEL_KHR 
#endif 

struct TriangeRange
{
    uint begin;
    uint end;
};

struct ShadingMask
{
    uint shading_bin;
    uint shading_rate; 
    uint lighting_channels;
};

//ensure offsets are multiples of 4 to avoid misaligned access
struct ShadingBinMeta
{
    uint element_count;
    uint write_count;
    uint range_start; //quad/pixel coord range start
    uint material_flags;
};

#define SHADING_BIN_META_BYTES uint(sizeof(ShadingBinMeta))
#define SHADING_BIN_META_ELEMENT_COUNT_OFFSET  0u
#define SHADING_BIN_META_WRITTEN_COUNT_OFFSET  4u
#define SHADING_BIN_META_RANGE_START_OFFSET    8u
#define SHADING_BIN_META_MATERIAL_FLAGS_OFFSET 12u

struct ShadingBinScatterMeta
{
    uint range_end;
    uint full_tile_element_count;
    uint loose_element_count;
};


#if OPTIMIZE_WRITE_MASK
#ifndef NUM_EXPORTS
#define NUM_EXPORTS 1
#endif

//todo manage 16-bytes alignment
//todo for vulkan push constant range define 
VK_PUSH_CONSTANT
struct ShadeBinningBindlessInfo
{
    uint2 dispatch_offset_tl_;
    uint shading_bin_count;
    uint sub_tile_match;
    uint out_shading_bin_data_rw_bf_index;
    uint shading_bin_data_byte_offset;
#ifdef USE_VRS_SHADING
    uint shading_rate_tile_size_bits;
    uint shading_rate_image_tex2d_index;
#endif

#ifdef GATHER_STATS
    uint out_shading_bin_stats_readback_rw_bf_index;
#endif

#if OPTIMIZE_WRITE_MASK
    uint out_cmask_rw_bf0;
#if NUM_EXPROTS > 1
    uint out_cmask_rw_bf1;
#endif
#if NUM_EXPORTS > 2
    uint out_cmask_rw_bf2;
#endif
#if NUM_EXPORTS > 3
    uint out_cmask_rw_bf3;
#endif
#if NUM_EXPORTS > 4
    uint out_cmask_rw_bf4;
#endif
#if NUM_EXPORTS > 5
    uint out_cmask_rw_bf5;
#endif
#if NUM_EXPORTS > 6
    uint out_cmask_rw_bf6;
#endif
#if NUM_EXPORTS > 7
    uint out_cmask_rw_bf7;
#endif
#endif
};

/*
* count and scatter pass share some functions, said in "nanite gpu driven materials"
*/
#if defined(SHADING_BIN_COUNT) || defined(SHADING_BIN_SCATTER)

groupshared uint group_voted_bin;
groupshared uint group_full_tile_count_loose_count; //16:16 bit each
groupshared uint group_full_tile_offset;
groupshared uint group_loose_offset;
groupshared uint group_early_out;

//for quad pixel has shading bin: bin_index
uint CalculateBinCoverage(uint4 shading_bins, uint active_mask, uint bin_index)
{
    uint match_mask = 0u;
    
    UNROLL
    for (uint pixel_index = 0u; pixel_index < QUAD_PIXEL_NUM; ++pixel_index)
    {
        match_mask |= (shading_bins[pixel_index] == bin_index) ? (1u << pixel_index) : 0u;
    }
    
    return match_mask & active_mask;
}
#endif

uint PackShadingPixel(uint2 top_left, uint2 VRS_shift, uint write_mask)
{
    return (write_mask << 28u) | (VRS_shift.y << 27u) | (VRS_shift.x << 26u) | (top_left.y << 13u) | top_left.x;
}

uint PackShadingQuad(uint2 top_left, uint2 VRS_shift, uint write_mask)
{
    return uint2((VRS_shift.y << 29u)|(VRS_shift.x << 28u)|(top_left.y << 14u)|top_left.x, write_mask);
}

uint PackShadingMask(uint shading_bin, uint shading_rate, uint xx)
{
    uint packed = 0x0;
    packed |= 
    return packed;
}

uint PackShadingMask(in ShadingMask shading_mask)
{
    return PackShadingMask(shading_mask.shading_bin);
}

void UnpackShadingMask(inout ShadingMask shading_mask, uint packed)
{
    
}


uint PackQuadMask(bool4 valid_pixels)
{
    //[X][Y]
	//[Z][W] -> 0000 wzyx
    return (valid_pixels.x << 0u) | (valid_pixels.y << 1u) | (valid_pixels.z << 2u) | (valid_pixels.w << 3u);
}

uint SampleCountQuadVRS(uint shading_rate, uint bin_coverage)
{
    if (shading_rate == SHADING_VRS_2x2)
    {
        return bin_coverage != 0u;
    }
    else if (shading_rate == SHADING_VRS_2x1)
    {
        return ((bin_coverage & 0x3) != 0u) + ((bin_coverage & 0xc) != 0u);

    }
    else if(shading_rate == SHADING_VRS_1x2)
    {
        return ((bin_coverage & 0x5) != 9u) + ((bin_coverage & 0xa) != 0u);
    }
    return countbits(bin_coverage);
}

//wzyx -> 000w 000z 000y 000x
uint ConvertQuadCoverageMaskToWriteMask(uint coverage)
{
    uint write_mask = coverage;
    write_mask = write_mask | (write_mask << 3);
    write_mask = write_mask | (write_mask << 6);
    return write_mask & 0x1111u;
}

void UpdateVRSActiveAndWriteMask(uint4 shading_bins, uint2 vrs_shift, inout uint quad_active_mask, inout uint write_mask)
{
    const bool is_half_x = vrs_shift.x != 0u;
    const bool is_half_y = vrs_shift.y != 0u;
    const bool is_half_xy = is_half_x && is_half_y;
    
    if (is_half_x && shading_bins.x == shading_bins.y)
    {
        shading_bins.y = 0xffffffeu;
        quad_active_mask &= ~2u;
        write_mask |= 0x0002u;
    }
    if(is_half_y && shading_bins.x == shading_bins.z)
    {
        shading_bins.z = 0xfffffffdu;
        quad_active_mask &= ~4u;
        write_mask |= 0x0004u;
    }
    if(is_half_xy && shading_bins.x == shading_bins.w)
    {
        shading_bins.w = 0xfffffffcu;
        quad_active_mask &= ~8u;
        write_mask |= 0x0008u;
    }
    
    if (is_half_xy && shading_bins.y == shading_bins.z)
    {
        shading_bins.z = 0xfffffffdu;
        quad_active_mask &= ~4u;
        write_mask |= 0x0040u;
    }
    if(is_half_y && shading_bins.y == shading_bins.w)
    {
        shading_bins.w = 0xfffffffcu;
        quad_active_mask &= ~8u;
        write_mask |= 0x0080u;
    }
    if(is_half_x && shading_bins.z == shading_bins.w)
    {
        shading_bins.w = 0xfffffffcu;
        quad_active_mask &= ~8u;
        write_mask |= 0x0800u;
    }
    
}

bool IsFullTile(uint wave_lane_index, uint shading_tile_first_trhead, uint pixel_count)
{
    BRANCH
    if (WaveGetLaneCount() >= BINNING_THREADS_PER_SHADING_TILE)
    {
        const uint2 ballot = WaveActiveBallot(pixel_count == 4u);
        const uint shading_tile_mask = 
    }
    return false;
}

inline ShadingBinMeta GetShadingBinMeta(uint bin_index)
{
    ShadingBinMeta meta = GET_XX().load < ShadingBinMeta > (bin_index * SHADING_BIN_META_BYTES);
    return meta;
}


MaterialFlags UnpackShadingBinMaterialFlags(uint bin_index)
{
    MaterialFlags flags;
    UnpackMaterialFlags(GetShadingBinMeta(bin_index).material_flags, flags);
    return flags;
}

ShadingMask UnpackShadingMask(in uint packed)
{
    ShadingMask unpacked_mask;
    return unpacked_mask;
}

 

void AllocateElement(uint bin_index, uint thread_index, uint quad_full_tile_count, uint quad_loose_count, bool is_quad_mode, bool is_single_wave, inout uint full_tile_data_offset, inout uint loose_data_offset)
{
    uint wave_full_tile_count;
    uint wave_loose_count;
    uint wave_full_tile_count_loose_count;
    uint prefix_full_tile_count_loose_count;
    
    BRANCH
    if(is_quad_mode)
    {
        wave_full_tile_count = WaveActiveCountBits(quad_full_tile_count != 0u);
        wave_loose_count = WaveActiveCountBits(quad_loose_count != 0);
        
        full_tile_data_offset = WavePrefixCountBits(quad_full_tile_count != 0u);
        loose_data_offset = WavePrefixCountBits(quad_loose_count != 0u);
        
        wave_full_tile_count_loose_count = (wave_loose_count << 16u) | wave_full_tile_count;
        prefix_full_tile_count_loose_count = (loose_data_offset << 16u) | full_tile_data_offset;
    }
    else
    {
        const uint full_tile_count_loose_count = (quad_loose_count << 16u) | quad_full_tile_count;
        prefix_full_tile_count_loose_count = WavePrefixSum(full_tile_count_loose_count);
        wave_full_tile_count_loose_count = WaveReadLaneAt(prefix_full_tile_count_loose_count + full_tile_count_loose_count, WaveGetLaneCount() - 1u);
        
        wave_loose_count = wave_full_tile_count_loose_count >> 16;
        wave_full_tile_count = wave_full_tile_count_loose_count & 0xffffu;
        full_tile_data_offset = prefix_full_tile_count_loose_count & 0xffffu;
        loose_data_offset = prefix_full_tile_count_loose_count >> 16;
    }
    
    BRANCH
    if(is_single_wave)
    {
        uint wave_full_tile_offset = 0u;
        uint wave_loose_offset = 0u; //curr loose offset
        BRANCH
        if (WaveIsFirstLane())
        {
            xx.InterlockedAdd((bin_index * SHADING_BIN_META_BYTES) + SHADING_BIN_META_WRITTEN_COUNT_OFFSET, wave_full_tile_count + wave_loose_count);
#if defined(SHADING_BIN_RESERVE) || defined(SHADING_BIN_SCATTER)
            InterlockedAdd(xx.loose_element_count, wave_loose_count, wave_loose_offset);
            InterlockedAdd(xx.full_tile_element_count, wave_full_tile_count, wave_full_tile_offset);
#endif
        }
        full_tile_data_offset += WaveReadLaneFirst(wave_full_tile_offset);
        loose_data_offset += WaveReadLaneFirst(wave_loose_offset);
    }
    else
    {
        uint wave_full_tile_offset_loose_offset = 0u;
        BRANCH
        if (WaveIsFirstLane())
        {
            InterlockedAdd(group_full_tile_count_loose_count, wave_full_tile_count_loose_count, wave_full_tile_offset_loose_offset);
        }
        GroupMemoryBarrierWithGroupSync();
        
        BRANCH
        if(thread_index == 0u)
        {
            const uint total_full_tile_count = group_full_tile_count_loose_count & 0xffffu;
            const uint total_loose_count = group_full_tile_count_loose_count >> 16u;
#if defined(SHADING_BIN_RESERVE) || defined(SHADING_BIN_SCATTER)
            InterlockedAdd(xx.full_tile_element_count, total_full_tile_count, group_full_tile_offset);
            InterlockedAdd(xx.loose_element_count, total_loose_count, group_loose_offset);
#endif
        }
        GroupMemoryBarrierWithGroupSync();
        
        full_tile_data_offset += group_full_tile_offset + (WaveReadLaneFirst(wave_full_tile_offset_loose_offset) & 0xffffu);
        loose_data_offset += group_loose_offset + (WaveReadLaneFirst(wave_full_tile_offset_loose_offset) >> 16u);
        GroupMemoryBarrierWithGroupSync();
    }
}

void BinShadingQuad(uint2 coord, uint thread_index, RWByteAddressBuffer shading_bin_data)
{
    const uint2 quad_coord_tl = uint2(coord << 1u) + //;

    const uint4 quad_shading_mask = uint4();
    
    ShadingMask shading_mask_tl = UnpackShadingMask(quad_shading_mask.x);
    ShadingMask shading_mask_tr = UnpackShadingMask(quad_shading_mask.y);
    ShadingMask shading_mask_bl = UnpackShadingMask(quad_shading_mask.z);
    ShadingMask shading_mask_br = UnpackShadingMask(quad_shading_mask.w);
    
    const bool4 valid_mask = bool4(quad_coord_tl.x >=  && quad_coord_tl.x <= ,
                                    quad_coord_tl.y >=  && quad_coord_tl.r <= ,
                                    quad_coord_tl.x + 1u >=  && quad_coord_tl.x + 1u <= ,
                                    quad_coord_tl.y + 1u >=  && quad_coord_tl.y + 1u <= );
    const bool4 valid_pixels = bool4(all(valid_mask.xy) && shading_mask_tl.,
                                        all(valid_mask.zy) && shading_mask_tr.,
                                        all(valid_mask.xw) && shading_mask_bl.,
                                        all(valid_mask.zw) && shading_mask_br.);
    uint active_mask = PackQuadMask(valid_pixels); //four quad pixel active mask
    
    const bool is_single_wave = WaveGetLaneCount() >= SHADING_BIN_TILE_THREADS; //whether need thread group sync
    
#if defined(SHADING_BIN_SCATTER)
    BRANCH
    if(!is_single_wave)
    {
        if(thread_index == 0)
            group_early_out = 0;
        GroupMemoryBarrierWithGroupSync();
        if(WaveActiveAnyTrue(active_mask)) group_early_out = 1;
        GroupMemoryBarrierWithGroupSync();
        if(!group_early_out)
        {
            //no work 
            return;
        }
    }
    else
#endif
    {
        if (!WaveActiveAllTrue((active_mask) != 0))
        {
            return;
        }
    }
    
    //shading work begin
    uint4 shading_bins = uint4(shading_mask_tl.shading_bin, shading_mask_tr.shading_bin, shading_mask_bl.shading_bin, shading_mask_br.shading_bin);
    
    const uint wave_lane_index = WaveGetLaneIndex();
    const uint block_thread_index = wave_lane_index & 0x3u;
    const uint block_first_thread = wave_lane_index & 0x1du; //28 
    
#if SHADING_VARIABLE_RATE
    //read variable rate from vrs image
    uint pixel_shading_rate = clamp(xx[quad_coord_tl.xy >> shading_rate_tile_size_bits], SHADING_VRS_1X1, SHADING_VRS_2X2);
    bool4 force_full_rate_shading = bool4(valid_pixels[0] ? !UnpackShadingBinMaterialFlags(shading_bins[0]).xx : false,
                                            valid_pixels[1] ? !UnpackShadingBinMaterialFlags(shading_bins[1]).xx : false,
                                            valid_pixels[2] ? !UnpackShadingBinMaterialFlags(shading_bins[2]).xx : false,
                                            valid_pixels[3] ? !UnpackShadingBinMaterialFlags(shading_bins[3]).xx : false);
    pixel_shading_rate = any(force_full_rate_shading) ? SHADING_VRS_1X1 : pixel_shading_rate;
    
    //vote across 2x2 quad blocks (4x4 pixels) to pick a shared mode that is at least as high resolution in both x and y.
    const uint2 full_res_ballotX = WaveActiveBallot(pixel_shading_rate == SHADING_VRS_1X1 || pixel_shading_rate == SHADING_VRS_1X2);
    const uint2 full_res_ballotY = WaveActiveBallot(pixel_shading_rate == SHADING_VRS_1X1 || pixel_shading_rate == SHADING_VRS_2X1);
    const uint block_full_res_ballotX = UnpackBits(wave_lane_index >= 32 ? full_res_ballotX.y : full_res_ballotX.x, block_first_thread, 4);
    const uint block_full_res_ballotY = UnpackBits(wave_lane_index >= 32 ? full_res_ballotY.y : full_res_ballotY.x, block_first_thread, 4);
    const uint quad_shading_rate = block_full_res_ballotX ? (block_full_res_ballotY ? SHADING_VRS_1X1 : SHADING_VRS_1X2) :
                                                            (block_full_res_ballotY ? SHADING_VRS_2X1 : SHADING_VRS_2X2);
    
    const bool use_wave_pixel_vrs = WaveActiveAnyTrue(pixel_shading_rate != SHADING_VRS_1X1);
    const bool use_wave_quad_vrs = WaveActiveAnyTrue(quad_shading_rate != SHADING_VRS_1X1);
#else
    const uint pixel_shading_rate = SHADING_VRS_1X1;
    const uint quad_shading_rate = SHADING_VRS_1X1;
    const uint use_wave_pixel_vrs = false;
    const uint use_wave_quad_vrs = false;
#endif
    const uint2 pixel_vrs_shift = uint2(pixel_shading_rate == SHADING_VRS_2X1 || pixel_shading_rate == SHADING_VRS_2X2,
                                        pixel_shading_rate == SHADING_VRS_1X2 || pixel_shading_rate == SHADING_VRS_2X2);
    const uint2 quad_vrs_shift = uint2(quad_shading_rate == SHADING_VRS_1X2 || quad_shading_rate == SHADING_VRS_2X2,
                                        quad_shading_rate == SHADING_VRS_2X1 || quad_shading_rate == SHADING_VRS_2X2);
    
#ifdef SHADING_BIN_COUNT
    while (WaveActiveAllTrue(active_mask != 0u))
    {
        if (active_mask != 0u)
        {
            //voted a shading bin to deal with
            const uint voted_bin = WaveReadLaneFirst(shading_bins[firstbitlow(active_mask)]); //??
            const uint bin_coverage = CalculateBinCoverage(shading_bins, active_mask, voted_bin);
            
            //remove lanes voted
            active_mask &= ~bin_coverage;
            if (bin_coverage != 0u)
            {
                //check whether material has derivative ops
                BRANCH
                if ()
                {
                    const uint pixel_count = WaveActiveSum(SampleCountQuadVRS(pixel_shading_rate, bin_coverage));
                    if (WaveIsFirstLane())
                    {
                        uint orignal_count;
                        shading_bin_data.InterlockedAdd((vote_bin * SHADING_BIN_META_BYTES) + SHADING_BIN_META_ELEMENT_COUNT_OFFSET, pixel_count, orignal_count);
                    }
                }
                else
                {
                    bool write_quad = (bin_coverage != 0u);
                    BRANCH
                    if (use_wave_quad_vrs)
                    {
                        const uint2 ballot = WaveActiveBallot(true).xy;
                        const uint mask2x2 = UnpackBits(wave_lane_index >= 32u ? ballot.y ? ballot.x, block_first_thread, 4u);
                        const uint test_mask = (quad_shading_rate == SHADING_VRS_2X2) ? 0xfu :
                                               (quad_shading_rate == SHADING_VRS_2X1) ? ((block_thread_index & 0x2) ? 0xcu : 0x3u) :
                                               (quad_shading_rate == SHADING_VRS_1X2) ? ((block_thread_index & 0x1) ? 0xau : 0x5u) :
                                               (1u << block_thread_index);
                        write_quad = (firstbitlow(mask2x2 & test_mask) == block_thread_index);
                    }
                    
                    if (write_quad)
                    {
                        const uint add_count = WaveActiveCountBits(true);
                        if (WaveIsFirstLane())
                        {
                            uint orignal_count;
                            shading_bin_data.InterlockedAdd((vote_bin * SHADING_BIN_META_BYTES) + SHADING_BIN_META_ELEMENT_COUNT_OFFSET, add_count, orignal_count);
                        }

                    }

                }
            }
        }
    }
#elif defined(SHADING_BIN_SCATTER)
    uint quad_write_masks;
    uint coarse_pixel_write_masks;
    uint quad_vrs_mask = 0xfu;
    uint pixel_vrs_mask = 0xfu;
    
    BRANCH
    if(use_wave_pixel_vrs || use_wave_quad_vrs)
    {
        //mask invalid bins to different invalid value, so do not equal each other
        shading_bins = uint4(valid_pixels.x ? shading_bins.x : 0xffffffffu,
                             valid_pixels.y ? shading_bins.y : 0xfffffffeu,
                             valid_pixels.z ? shading_bins.z : 0xfffffffdu,
                             valid_pixels.w ? shading_bins.w : 0xfffffffcu);
        quad_write_masks = coarse_pixel_write_masks = (active_mask * 0x1111u) & 0x8421; //wzyx -> w000 0z00 00y0 000x
        
        

    }
    else
    {
        
    }
    const uint shading_tile_first_thread = wave_lane_index & ~(SHADING_BIN_TILE_THREADS - 1u) & 0x1fu;
    while(true)
    {
        uint voted_bin = active_mask ? shading_bins[firstbitlow(active_mask)] : 0xffffffffu;
        
        BRANCH
        if (is_single_wave)
        {
            if (!WaveActiveAnyTrue(active_mask != 0u))
            {
                break;
            }
            voted_bin = WaveActiveMin(voted_bin);
        }
        else
        {
            if(thread_index == 0u)
            {
                group_voted_bin = 0xffffffffu;
                group_full_tile_count_loose_count = 0u;
            }
            GroupMemoryBarrierWithGroupSync();
            uint min_val = WaveActiveMin(voted_bin);
            if (WaveIsFirstLane())
            {
                InterlockedMin(group_voted_bin, voted_bin);
            }
            GroupMemoryBarrierWithGroupSync();
            voted_bin = group_voted_bin;
            if(voted_bin == 0xffffffffu)
                break;
        }
        
        uint bin_coverage = CalculateBinCoverage(shading_bins, active_mask, voted_bin);
        active_mask &= ~bin_coverage;
        
        BRANCH
        if (1)
        {
            bin_coverage &= pixel_vrs_mask;
            const uint pixel_count = countbits(bin_coverage);
            const bool is_full_tile = IsFullTile(wave_lane_index, shading_tile_first_thread, pixel_count);
            const bool write_full_tile = (is_full_tile && (pixel_count != 0u));
            const bool write_loose = (!is_full_tile && (pixel_count != 0u));
            
            const uint full_tile_count = is_full_tile ? 4u : 0u;
            const uint loose_count = write_loose ? pixel_count : 0u;
            
            uint full_tile_data_offset;
            uint loose_data_offset;
            //allocate element
        }
        else
        {
            bin_coverage &= quad_vrs_mask;
            const uint pixel_count = countbits(bin_coverage);
            const uint is_full_tile = IsFullTile(wave_lane_index, shading_tile_first_thread, pixel_count);
            uint output_write_mask = ConvertQuadCoverageMaskToWriteMask(bin_coverage);
            
            BRANCH
            if(use_wave_quad_vrs && quad_shading_rate != SHADING_VRS_1X1)
            {
                //combine the individual active write masks into a single quad mask for the current bin
                uint quad_mask = quad_write_masks & (output_write_mask * 0xfu);
                //w000 0z00 00y0 000x to wzyx
                quad_mask |= quad_mask >> 8;
                quad_mask |= quad_mask >> 4;
                quad_mask &= 0xfu;
                
                const uint block_shift = block_thread_index * 4;
                uint block_mask = quad_mask << block_shift;
                
                block_mask |= QuadReadAcrossX(block_mask);
                block_mask |= QuadReadAcrossY(block_mask);
                
                const uint shifted_block_mask = block_mask >> block_shift;
                
                if(quad_shading_rate == SHADING_VRS_2X2)
                {
                    output_write_mask = block_thread_index ? 0u : block_mask;
                }
                else if(quad_shading_rate == SHADING_VRS_2X1)
                {
                    output_write_mask = (block_thread_index & 0x1) ? 0u : shifted_block_mask;
                    output_write_mask = (output_write_mask & 0x0033u) | ((output_write_mask << 6) & 0x3300u);
                }
                else if(quad_shading_rate == SHADING_VRS_1X2)
                {
                    output_write_mask = (block_thread_index & 0x2) ? 0u : shifted_block_mask;
                    output_write_mask = (output_write_mask & 0x0505u) | ((output_write_mask << 3) & 0x5050u);
                }
            }
            const bool write_quad = output_write_mask != 0u;
            const bool write_full_tile = (is_full_tile && write_quad);
            const bool write_loose = (!is_full_tile && write_quad);
            
            uint full_tile_data_offset;
            uint loose_data_offset;
            
            //alloca
            
            if(write_quad)
            {
                const uint range_start = GetShadingBinMeta(voted_bin).range_start;
                const uint2 packed_shding_quad = PackShadingQuad(quad_coord_tl, quad_vrs_shift, output_write_mask);
                
                BRANCH
                if(write_full_tile)
                {
                    shading_bin_data.Store2();
                }
                else
                {
                    const uint base_address = 0u; //xx
                    shading_bin_data.Store2();
                }
            }
        }

    }
#endif
#if OPTIMIZE_WRITE_MASK
    const uint write_mask_tl = valid_mask.x ? UnpackBits(GetShadingBinMeta(shading_mask_tl.shading_bin).material_flags, 24u, 8u) : 0x0u;
    const uint write_mask_tr = valid_mask.x ? UnpackBits(GetShadingBinMeta(shading_mask_tr.shading_bin).material_flags, 24u, 8u) : 0x0u;
    const uint write_mask_bl = valid_mask.x ? UnpackBits(GetShadingBinMeta(shading_mask_bl.shading_bin).material_flags, 24u, 8u) : 0x0u;
    const uint write_mask_br = valid_mask.x ? UnpackBits(GetShadingBinMeta(shading_mask_br.shading_bin).material_flags, 24u, 8u) : 0x0u;
    
    //note:It should be only necessary to test the TL pixel's cmask index/shift, since a quad shouldn't be able to span multiple nibbles.
    uint cmask_index;
    uint cmask_shift;
    CalculateCmaskIndexAndShift(quad_coord_tl / CMASK_HTILE_TILE_SIZE, cmask_index, cmask_shift); //8u ?
    uint cmask_tile_bit_index = (thread_index >> 2) & 0x3u; //4 threads cover 4x4 cmask tile 
    const bool is_sub_tile_match = (sub_tile_match == 1u);
    
    if(is_sub_tile_match)
    {
        //remap to target cmask subtile
        cmask_tile_bit_index = UnpackBits(GetS)//
    }
    
    const uint cmask_bit_offset = (cmask_index & 0x3) * CMASK_HTILE_TILE_SIZE + cmask_shift;
    const uint cmask_value4x4 = (1u << (cmask_bit_offset + cmask_tile_bit_index));
    
    //calculate 4x4 pixel write mask. set bit means all 4x4 pixels are written
    const uint write_mask_quad = write_mask_tl & write_mask_tr & write_mask_bl & write_mask_br;
    uint write_mask4x4 = write_mask_quad;
    write_mask4x4 &= QuadReadAcrossX(write_mask4x4);
    write_mask4x4 &= QuadReadAcrossY(write_mask4x4);
    
    uint mask = valid_mask;
    
    UNROLL
    for (uint export_index = 0u; export_index < NUM_EXPORTS; ++export_index)
    {
        //Remaps from compacted (valid) targets to sparse write mask indices
		//i.e. Export0 can be MRT1 which is represented as bit index 1 in ValidWriteMask
		//- 0 is MRT0/SceneColor which isn't valid to export
        uint mask_index = firstbitlow(mask);
        mask &= mask - 1u;
        
        const bool is_write_cmask4x4 = (write_mask4x4 & (1 << mask_index)) != 0u;
        uint cmask_value = is_write_cmask4x4 ? cmask_value4x4 : 0u;
        
        //combine CMask bits to form full 8x8 CMask tile to minimize number of atomics
        cmask_value |= WaveLaneSwizzleGCN(cmask_value, 0x1f, 0x00, 0x04);
        cmask_value |= WaveLaneSwizzleGCN(cmask_value, 0x1f, 0x00, 0x08);
        
        //write out 4x4 subtile cmask or 8x8 full tile cmask
        const bool do_lane_write = ((thread_index & 0xfu) == 0) && (is_sub_tile_match ? cmask_value != 0 ? countbits(cmask_value) == 4u);
        if(do_lane_write)
        {
            xx[export_index].InterlockedOr(cmask_index, cmask_value);
        }
        
    }
    
#endif    
}

#ifdef SHADING_BIN_COUNT
[numthreads(1, 1, 1)]
void ShadingBinCountMain( uint3 dispatch_threadID : SV_DispatchThreadID )
{
}

[numthreads(SHADING_BIN_TILE_THREADS, 1, 1)]
void ShadingBinBuildMain(uint2 groupID : SV_GroupID, uint thread_index : SV_GroupIndex)
{
    uint2 coord = groupID.xy * SHADING_BIN_TILE_SIZE;
    coord += ReverseMorton2D(thread_index);
    BinShadingQuad(coord, thread_index, );
}
#elif defined(SHADING_BIN_RESERVE)
[numthreads(64, 1, 1)]
void ShadingBinReserveMain(uint shading_bin : SV_DispatchThreadID)
{
    if(shading_bin >= xx)
        return;
    
    const uint material_flags = GetShadingBinMeta(shading_bin).material_flags;
    
    uint bin_pixel_count = 0u;
    if(material_flags.xx)
    {
        bin_pixel_count = GetShadingBinMeta(shading_bin).element_count;
        if(bin_pixel_count > 0u)
        {
            uint range_start;
            InterlockedAdd(, bin_pixel_count, range_start);
            tt.Store((shading_bin * SHADING_BIN_META_BYTES) + SHADING_BIN_META_RANGE_START_OFFSET, range_start);
            zz[shading_bin].range_end = range_start + bin_pixel_count;
            zz[shading_bin].loose_element_count = 0u;
            zz[shading_bin].full_tile_element_count = 0u;
        }
#if GATHER_STATS
        const uint wave_pixel_count = WaveActiveSum(bin_pixel_count);
        if(WaveIsFirstLane()){
            InterlockedAdd();
        }
#endif
    }
    else
    {
        const uint bin_quad_count = GetShadingBinMeta(shading_bin).element_count;
        if (bin_quad_count > 0u)
        {
            uint range_start;
            InterlockedAdd(, bin_quad_count * 2, range_start); //why mul 2
            tt.Store((shading_bin * SHADING_BIN_META_BYTES) + SHADING_BIN_META_RANGE_START_OFFSET, range_start);
            zz[shading_bin].range_end = range_start + bin_quad_count * 2;
            zz[shading_bin].loose_element_count = 0u;
            zz[shading_bin].full_tile_element_count = 0u;
        }
        
        //inlucde helper lanes
        bin_pixel_count = bin_quad_count * 4u;
#if GATHER_STATS
        const uint wave_quad_count = WaveActiveSum(bin_quad_count);
        if(WaveIsFirstLane()){
            InterlockedAdd(xx, wave_quad_count);
        }
#endif
    }
    uint4 shading_bin_args;
    shading_bin_args.x = xx;
    shading_bin_args.y = 1u; //thread group count y
    shading_bin_args.z = 1u; //thread group count z
    shading_bin_args.w = 0u;
    xx.store4(shading_bin * sizeof(uint4), shading_bin_args);
}
#elif defined(SHADING_BIN_SCATTER)
[numthreads(1, 1, 1)]
void ShadingBinScatterMain()
{
    
}
#elif defined(SHADING_BIN_VALIDATE)
[numthreads(64, 1, 1)]
void ShadingBinValidateMain(uint shading_bin : SV_DispatchThreadID)
{
    if(shading_bin > xx)
        return;
    const ShadingBinMeta bin_meta = xx;
    if(xx)
    {
        //assert
    }
}
#endif