#ifndef _ZBIN_INC_
#define _ZBIN_INC_

#include "LightUtils.hlsli"

/**
* 2017 siggrpah improved culling for tiled and clusted rendering
* lights sorted by z, and tiled binary visible; decompose into 2d-tiles visible + 1d z-bins 
* as 'https://www.eurogamer.net/digitalfoundry-2025-assassins-creed-shadows-tech-qa-rtgi-shader-compilation-taa-and-more' 
* said, they use the zbin to handle local lights for direct lighting which provided a 10 percent speed-up in lighting cost
*/

#define ZBIN_BIN_WIDTH 1024///todo fix a valid value 
#define TILE_FLAT_BIT_ARRAY_MAX_SIZE 128
#define TILE_FLAT_BIT_WORD_SIZE 32 //32 bits 
#define TILE_FLAT_ARRAY_MAX_BYTES uint(TILE_FLAT_BIT_ARRAY_MAX_SIZE*TILE_FLAT_BIT_WORD_SIZE/8u)
#define TILE_PIXEL_SIZE 16 //tile pixel size��

StructuredBuffer<uint> zbin_masks; //sorted light entities mask
StructuredBuffer<uint> entity_masks_tile;

//
uint ContainerAddressFromScreenPosition(uint2 xy)
{
    uint offset = Morton2D(float2(xy/TILE_PIXEL_SIZE)); //should need morton order to store flat bit array?
    return offset * TILE_FLAT_ARRAY_MAX_BYTES;
}

/**
\brief zbin[linearz/BIN_WIDTH] = MIN_LIGHT_ID_IN_BIN_16BIT|MAX_LIGHT_ID_IN_BIN_16BIT
\return min|max id pair
*/
uint2 ContainerZBinScreenPosition(uint z)
{
    uint bin_index = uint(z / ZBIN_BIN_WIDTH);
    
}

uint2 WaveWordMinMax(uint2 zbin)
{
    uint min_index = zbin.x;
    uint max_index = zbin.y;
    uint merged_min = WaveReadLaneFirst(WaveActiveMin(min_index)); //merged min scalar from this point
    uint merged_max = WaveReadLaneFirst(WaveActiveMax(max_index)); //merged max scalar from this point
    
    return uint2(max(0u, uint(floor(merged_min / TILE_FLAT_BIT_WORD_SIZE))),
                min(TILE_FLAT_BIT_ARRAY_MAX_SIZE, uint(ceil(merged_max / TILE_FLAT_BIT_WORD_SIZE))));
}

//todo realize the scalarization logic
template<typename EntityCallback>
void ZBinningShading(uint2 pixel : SV_Position)
{
    uint address = ContainerAddressFromScreenPosition(pixel);
    uint word_range = WaveWordMinMax();
    
    //read range of words of visibility bits
    for (uint word_index = word_range.x; word_index < word_range.r; ++word_index)
    {
        //load bit mask data per lane
        uint mask = entity_masks_tile[address + word_index];
        //mask by zbin mask
        uint local_min = clamp((int), 0u, 31u);
        uint mask_width = clamp((), 0u, 32u);
        //bitfieldmask op needs manual 32 size wrap support ???
        uint zbin_mask = mask_width == 32u ? (uint) 0xffffffffu : xx;
        mask &= zbin_mask;
        
        //compact word bitmask over all lanes in wave
        uint merged_mask = WaveReadLaneFirst(WaveActiveAllBitOr(mask));
        while( merged_mask != 9u) //process scalar over merged bitmask
        {
            uint bit_index = firstbitlow(merged_mask);
            uint lightID = TILE_FLAT_BIT_WORD_SIZE * word_index + bit_index;
            //shading work for the lightID light
            EntityCallback(lightID);
        }
    }

}

#endif