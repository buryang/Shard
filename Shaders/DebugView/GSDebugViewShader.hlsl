#include "DebugViewCommon.hlsli"

VK_PUSH_CONSTANT
GSParams line_params;

[maxvertexcount(6)]
void mainLineWidth(line float4 input[2] : POSITION0,  out TriangleStream< GSOutput > output)
{
    float4 p0 = input[0];
    float4 p1 = input[1];
    float w0 = p0.w;
    float w1 = p1.w;
    p0.xyz /= w0;
    p1.xyz /= w1;
    
    float3 line01 = p1 - p0;
    float3 dir = normalize(line01);
    
    // scale to correct window aspect ratio
    float3 ratio = float3(line_params.render_target_size.y, line_params.render_target_size.x, 0);
    ratio = normalize(ratio);
    float3 unit_z = normalize(float3(0, 0, -1)); //z tangent
    float3 normal = normalize(cross(unit_z, dir) * ratio);

        GSOutput v[4];
    float3 dir_offset = dir * ratio * line_params.line_width;
    float3 normal_scaled = normal * ratio * line_params.line_width;

    float3 p0_ex = p0 - dir_offset;
    float3 p1_ex = p1 + dir_offset;

    v[0].pos = float4(p0_ex - normal_scaled, 1) * w0;
    v[1].pos = float4(p0_ex + normal_scaled, 1) * w0;
    v[2].pos = float4(p1_ex + normal_scaled, 1) * w1;
    v[3].pos = float4(p1_ex - normal_scaled, 1) * w1;
    
    output.Append(v[2]);
    output.Append(v[1]);
    output.Append(v[0]);
    output.RestartStrip();
   
    output.Append(v[3]);
    output.Append(v[2]);
    output.Append(v[0]);
    output.RestartStrip();
}