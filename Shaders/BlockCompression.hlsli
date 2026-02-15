#ifndef _TEXTURE_COMPRESSION_HLSLI
#define _TEXTURE_COMPRESSION_HLSLI

// Texture compression functions and definitions go here

//BC1 helper function used in slide "https://static.graphicsprogrammingconference.com/public/2025/slides/how-to-decimate-your-textures/Malan-how-to-decimate-your-textures-bcn-compression-tricks-in-horizon-forbidden-west.pdf”
//for more information, read https://fgiesen.wordpress.com/2021/10/04/gpu-bcn-decoding/ 

float3 ExpandRGB565(uint packedColor)
{
    // Extract the individual color components
    uint r = (packedColor >> 11) & 0x1F; // 5 bits for red
    uint g = (packedColor >> 5) & 0x3F;  // 6 bits for green
    uint b = packedColor & 0x1F;         // 5 bits for blue

    //expeand to 8-bit with bit replcation， slide p.21
    float3 color;
    color.r = (r << 3) | (r >> 2); // Replicate bits to fill 8 bits
    color.g = (g << 2) | (g >> 4); // Replicate bits to fill 8 bits
    color.b = (b << 3) | (b >> 2); // Replicate bits to fill 8 bits
    return color/255.f;
}

void DecodeBC1InterpolatedColorsAMD(uint2 block, out float3 interp_colors[4])
{
    uint c0 = block.x & 0xFFFF;
    uint c1 = block.x >> 16;
    uint indices = block.y;

    interp_colors[0] = ExpandRGB565(c0);
    interp_colors[1] = ExpandRGB565(c1);

    bool four_color_mode = c0 > c1;

    if (four_color_mode)
    {
        // 2/3 and 1/3 weights (AMD: (43*c0 + 21*c1 + 32)/64 ≈ (2/3 c0 + 1/3 c1)
        interp_colors[2] = ((43.0f * c0) + (21.0f * c1) + 32.0f.xxx) / 64.0f;
        interp_colors[3] = ((21.0f * c0) + (43.0f * c1) + 32.0f.xxx) / 64.0f;

    }
    else
    {
        // 1/2 weight + black
        interp_colors[2] = (c0 + c1 + 1.0f.xxx / 255.0f) / 2.0f;  // Approximate rounding
        interp_colors[3] = 0.0f.xxx;  // Black (transparent in alpha mode)

    }
}

void DecodeBC1InterpolatedColorosNVIDIA(uint2 block, out float3 interp_colors[4])
{
    //todo
}

void DecodeBC1Block(uint2 block, out float3 colors[16])
{
    float3 interp_colors[4];
#ifdef GPU_VENDOR_AMD
    DecodeBC1InterpolatedColorsAMD(block, interp_colors);
#elif GPU_VENDOR_NVIDIA
    DecodeBC1InterpolatedColorsNVIDIA(block, interp_colors);
#else 
    //todo
#endif
    for (int i = 0; i < 16; ++i)
    {
        uint index = (block.y >> (2 * i)) & 0x03; // Extract 2-bit index
        colors[i] = interp_colors[index];
    }
}

//RGB to Scalar in slice just dot float3(0.96414679, 0.03518212, 0.00067109)


#endif // _TEXTURE_COMPRESSION_HLSLI
