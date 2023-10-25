#ifndef _BVH_COMMON_INC_
#define _BVH_COMMON_INC_

//https://developer.nvidia.com/blog/thinking-parallel-part-iii-tree-construction-gpu/

//expand 10-bit integer to 30 bits
uint ExpandBits(uint3 v)
{
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

//calc morton code for the 3d coordinate located in uint cube[0,1]
uint Morton3D(float3 xyz)
{
    const float cube_scale = 1024.f;
    xyz = min(max(xyz * cube_scale, 0.f), cube_scale-1.f);
    uint3 expand_xyz = ExpandBits(uint3(xyz));
    return expand_xyz.x * 4 + expand_xyz.y * 2 + expand_xyz.z;
}



#endif//_BVH_COMMON_INC_