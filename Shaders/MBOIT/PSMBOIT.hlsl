#include "../CommonUtils.hlsli"
#include "../BindlessResource.hlsli"

#if __SHADER_TARGET_MAJOR >= 5 && __SHADER_TARGET_MINOR >= 1
float4 MainMBOIT() : SV_TARGET
{
    RasterizerOrderedTexture2D<float4> moment_texture = GetBindlessROVTexture2D();
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
#endif