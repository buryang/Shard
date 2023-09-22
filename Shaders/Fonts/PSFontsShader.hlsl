#include "FontsShaderCommon.hlsli"

#ifdef FREE_TYPE_ALGO
sampler sampler0;
Texture2D texture0;
FontPSOutput main(FTVSOutput input) 
{
    FTPSOutput output;
    output.color = input.color * texture0.Sample(sampler0, input.uv).r;
    [likely]
    if(){
    }
    else 
    {

    }
    return output;
}
#else
FontPSOutput main(FTVSOutput input)
{
    FTPSOutput output;
    return output;
}
#endif