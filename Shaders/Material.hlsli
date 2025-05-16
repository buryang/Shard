#ifndef _MATERIAL_INC_
#define _MATERIAL_INC_

struct MaterialProperties
{
    float3 base_color_;
    float metalic_;
    
    float3 emissive_;
    float roughness_;
    //karis 2013
    float cavity_;
    
    float transmissiveness_;
    float relfectance_;
    float opacity_;
};

struct MaterialFlags
{
    
};

uint PackMaterialFlags(MaterialFlags flags)
{
    uint packed;
    return packed;
}

void UnpackMaterialFlags(uint packed, out MaterialFlags flags)
{
    
}


#endif //_MATERIAL_INC_ 