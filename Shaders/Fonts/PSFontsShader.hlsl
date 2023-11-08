#include "FontsShaderCommon.hlsli"

#ifdef FREE_TYPE_ALGO
sampler sampler0;
Texture2D texture0;
struct FontPSParams
{
    uint    with_sdf_;
    uint    soft_edges_;
    uint    outer_glow_;
    float   min_dist_;
    float   max_dist_;
};

VK_PUSH_CONSTANT
FontPSParams font_ps_params;

FontPSOutput Main(FTVSOutput input) 
{
    FTPSOutput output;
    [likely]
    //sdf distantance field algo follow this paper:
    //https://steamcdn-a.akamaihd.net/apps/valve/2007/SIGGRAPH2007_AlphaTestedMagnification.pdf 
    if(font_ps_params.with_sdf){
        output.color = input.color;
        float dist_alpha = texture0.Sample(sampler0, input.uv).a;
        [likely]
        if(font_ps_params.soft_edges_){
            dist_alpha = smoothstep(font_ps_params.min_dist_, font_ps_params.max_dist_, dist_alpha);
            output.color.a *= dist_alpha;
        }
        else 
        {
            output.color.a = dist_alpha >= 0.5f;
        }
        [unlikely]
        if(font_ps_params.outer_glow_){
            
        }
    }
    else 
    {
        output.color = input.color * texture0.Sample(sampler0, input.uv).r;
    }
    return output;
}
#else
FontPSOutput Main(FTVSOutput input)
{
    FTPSOutput output;
    return output;
}
#endif