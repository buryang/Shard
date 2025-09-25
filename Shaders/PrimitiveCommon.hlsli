#ifndef _PRIMITIVE_COMMON_INC_
#define _PRIMITIVE_COMMON_INC_
#include "../CommonUtils.hlsli"

//some geometry utility functions
inline float3 CalcTriangleNormal(float3 v0, float3 v1, float3 v2)
{
    return normalize(cross(v1 - v0, v2 - v0));
}

inline float3 CalcTriangleArea(float3 v0, float3 v1, float3 v2)
{
    return length(cross(v1 - v0, v2 - v0)) * 0.5f;
}

//check triangle backface in homogenous coordinates (clip space coordinate discard z)
inline bool IsTriangleBackFace(float4 vc0, float4 vc1, float4 vc2)
{
    float3x3 m = float3x3(vc0.xyw, vc1.xyw, vc2.xyw);
    /*In case the determinant is larger than 0, it has no inverse and therefore 
    * is back-facing and can be culled. If the determinant is 0, the triangle is 
    * degenerate, or is being viewed edge-on and has zero screen-space area. 
    * according to paper "Triangle Scan Conversion using 2D Homogeneous Coordinates"*/
    return determinant(m) >= 0;
}

//check triangle backface by normal
inline bool IsTriangleBackFace(float3 normal, float3 dir)
{
    return dot(normal, dir) <= 0;
}

//to do : Differential Barycentric Coordinates 

//compute the barycenrtic and its partial derivative of a triangle from the homogeneous clip space position（Perspective-Correct Interpolation）
//for high-precision; algorithm from appendix A of DAIS paper: "https://cg.ivd.kit.edu/publications/2015/dais/DAIS.pdf"
void CalcBaryAndDeriv(float4 vc0, float4 vc1, float4 vc2, float2 pixel_ndc, float2 screen_size, out float3 barycentric, out float3 dbdx, out float3 dbdy)
{
    float2 ndc0 = vc0.xy / vc0.w;
    float2 ndc1 = vc1.xy / vc1.w;
    float2 ndc2 = vc2.xy / vc2.w;
    float3 invW = rcp(float3(vc0.w, vc1.w, vc2.w));

    float invD = rcp(determinant(float2x2(ndc2 - ndc1, ndc0 - ndc1)));

    //ddx[i] = (y[i+1]-y[i-1])/D
    dbdx = float3(ndc1.y - ndc2.y, ndc2.y - ndc0.y, ndc0.y - ndc1.y)*invD*invW;
    dbdy = float3(ndc2.x - ndc1.x, ndc0.x - ndc2.x, ndc1.x - ndc0.x)*invD*invW;

    float dbdx_sum = dot(dbdx, float3(1,1,1)); // dot is faster than ddx.x + ddx.y + ddx.z; 
    float dbdy_sum = dot(dbdy, float3(1,1,1));

    float2 pixel_ndc_delt = pixel_ndc - ndc0;
    float interpInvW = invW.x + ddx_sum * pixel_ndc_delt.x + ddy_sum * pixel_ndc_delt.y;
    float interpW = rcp(interpInvW);

    barycentric.x = interpW * (invW.x + ddx.x * pixel_ndc_delt.x + ddy.x * pixel_ndc_delt.y);
    barycentric.y = interpW * (0.0f + ddx.y * pixel_ndc_delt.x + ddy.y * pixel_ndc_delt.y);
    barycentric.z = interpW * (0.0f + ddx.z * pixel_ndc_delt.x + ddy.z * pixel_ndc_delt.y);

    //scale from NDC to screen pixel
    dbdx *= screen_size.x;    
    dbdy *= screen_size.y;

    dbdx_sum *= screen_size.x;
    dbdy_sum *= screen_size.y;

    dbdy *= -1.0f; //y flip //why ?
    dbdy_sum *= -1.0f;

    // follow the implementation in TVB source code
    // fixes the derivative error happening for projected triangles
    // Instead of calculating the derivatives constantly across the 2D triangle we use a projected version
	// of the gradients, this is more accurate and closely matches GPU raster behavior.
	// final gradient equation: ddx = (((lambda/w) + ddx) / (w+|ddx|)) - lambda

    float interpW_ddx = 1.0f/(interpW + dbdx_sum);
    float interpW_ddy = 1.0f/(interpW + dbdy_sum);

    dbdx = interpW_ddx * (dbdx + barycentric * interpInvW) - barycentric;
    dbdy = interpW_ddy * (dbdy + barycentric * interpInvW) - barycentric;
}

//calculate attribute(1D, 2D, 3D) derivative from barycentric derivative
//column denotes attributes per vertex, as discribed in DAIS paper Appendix A
void CalcAttribute1DDeriv(float3 dbdx, float3 dbdy, float3 attributes, out float dadx, out float dady)
{
    dadx = dot(dbdx, attributes);
    dady = dot(dbdy, attributes);
}

void CalcAttribute2DDeriv(float3 dbdx, float3 dbdy, float3x2 attributes, out float2 dadx, out float2 dady)
{
    dadx.x = dot(dbdx, attributes[0]);
    dadx.y = dot(dbdx, attributes[1]);
    dady.x = dot(dbdy, attributes[0]);
    dady.y = dot(dbdy, attributes[1]);
}

void CalcAttribute3DDeriv(float3 dbdx, float3 dbdy, float3x3 attributes, out float3 dadx, out float3 dady)
{
    dadx = mul(attribute, dbdx);
    dady = mul(attribute, dbdy);
}

#endif //_PRIMITIVE_COMMON_INC_