#ifndef _RESTIR_COMMON_INC_
#define _RESTIR_COMMON_INC_

#include "../CommonUtils.hlsli"

struct ReSTIRReservoir
{
    float3 sample; //position or direction
    float weight_sum;
    float weight_square_sum;
    uint sample_count;
    float pdf; //pdf of the sample
    uint padding; //padding to 32 bytes
};

struct ReSTIRReservoirWithRadiance
{
    float3 sample; //position or direction
    float3 radiance; //radiance of the sample
    float weight_sum;
    float weight_square_sum;
    uint sample_count;
    float pdf; //pdf of the sample
    uint padding; //padding to 32 bytes
};


#endif //_RESTIR_COMMON_INC_