#ifndef _LIGHT_UTILS_INC_
#define _LIGHT_UTILS_INC_
#include "../CommonUtils.hlsli"

enum LightShapeType
{
    eInvlalid = 0,
    ePoint,
    eLine,
    eSphere,
    eCapsule,
    eRetangle,
    eNum,
};

//light shading related flags
#define LIGHT_FLAGS_VISIBLE 0x1u                            //visible（for GPU culling)
#define LIGHT_FLAGS_DIFFUSE_ENABLED 0x2u                    //diffuse lighting enabled
#define LIGHT_FLAGS_SPECULAR_ENABLED 0x4u                   //specular lighting enabled  
#define LIGHT_FLAGS_TRANSLUCENT_ENABLED 0x8u                //translucent lighting enabled
#define LIGHT_FLAGS_CAST_SHADOW 0x10u                       //cast shadow
#define LIGHT_FLAGS_SCREEN_SPACE_SHADOW 0x20u               //use screen space shadow 
#define LIGHT_FLAGS_COOKIES_ENABLED 0x40u                   //cookies enabled  
#define LIGHT_FLAGS_FRUSTUM_FEATHERING_ENABLED 0x80u        //frustum feathering enabled
#define LIGHT_FLAGS_INDEXED_LIGHT 0x100u                    //indexed baked lighting 

//tiled/clustered lighting related
#define MAX_LIGHT_PPER_TILE 64u

//https://github.com/Facepunch/sbox-public/blob/8b1d58d524c37fe287bef1674db4f4fa72f095f5/game/addons/base/Assets/shaders/common/lightbinner.hlsl

#endif 
