#ifndef _MATH_INC_
#define _MATH_INC_

#define M_PI 3.14159265359
#define M_INV_PI 0.3183098861
#define M_E 2.71828182845
#define M_EPSILON 1.2e-07f
#define M_GLODEN_RATIO_CONJUGATE 0.61803398875

#define MEDIUMP_FLT_MIN 0.00006103515625
#define MEDIUMP_FLT_MAX 65504.0
#define SaturateMediump(x) min(x, MEDIUMP_FLT_MAX)


//todo https://graphics.stanford.edu/~seander/bithacks.html
int CountBits(int val)
{
    //Counts the number of 1:s
    //https://www.baeldung.com/cs/integer-bitcount
    val = (val & 0x55555555) + ((val >> 1) & 0x55555555);
    val = (val & 0x33333333) + ((val >> 2) & 0x33333333);
    val = (val & 0x0F0F0F0F) + ((val >> 4) & 0x0F0F0F0F);
    val = (val & 0x00FF00FF) + ((val >> 8) & 0x00FF00FF);
    val = (val & 0x0000FFFF) + ((val >> 16) & 0x0000FFFF);
    return val;
}

//count bits nasjed to be the bits lower than index
uint CountBitsMasked(uint2 val, uint mask_index)
{
    bool is_lower = mask_index < 32;
    uint mask = 1u << (mask_index - (is_lower ? 0u : 32u));
    mask -= 1;
    uint offset;
    offset = countbits(val.x & (is_lower ? mask : ~0u));
    offset += countbits(val.y & (is_lower ? 0u : mask));
    return offset;
}

//code from "efficient coverage mask generation for antialiasing"
//@return unit angle 0-1 for -PI...PI
float Atan2Approx(float y, float x)
{
    return (y > 0) - sign(y) * (x / abs(x) + abs(y)) * 0.25f + 0.25f);
}

//fast approximation of arctangent 
float ArcTanApprox(float x)
{
    float y = x * (abs(x) * ( (M_PI * 0.5f) * abs(x) - 0.00507668f) + .420691f) /
              (abs(x) * (abs(x) * (.633387806077409f + abs(x)) + .671041944493641f) + .21519262713177476f);
    return y;
}

//&param unit angle 0-1 for -PI...PI
//&return approximates float2(sin(uint_angle * PI * 2 -PI), cos(uint_angle*PI * 2 - PI))
float2 SinCosApprox(float unit_angle)
{
    return normalize(float2(abs(abs(3 - 4 * unit_angle) - 2) - 1, 1 - abs(unit_angle * 4 - 2)));
}

//4th order approximation of acos for x
float AcosApprox4(float x)
{
    float x1 = abs(x);
    float x2 = x1 * x1;
    float x3 = x2 * x1;
    float s = -0.2121144*x1 + 1.5707288;
    s += 0.0742610*x2 - 0.0187293*x3;
    s *= sqrt(1.0 - x1);
    return x < 0.0 ? M_PI - s : s;
}

//higher precision rcp function
inline float RcpRefined(float val)
{
#if 1
    float r = rcp(val);
    return r * (2.0f - val * r); // One Newton-Raphson iteration
#else
    return 1.0f / (val + M_EPSILON);
#endif 
}

//higher precision rsqrt function
inline float RsqrtRefined(float val)
{
    float r = rsqrt(val);
    return 0.5f * r * (3.0f - val * r * r); // One Newton-Raphson iteration
}

/*------------noise function---------------*/
//https://www.shadertoy.com/view/4djSRW
//https://github.com/everyoneishappy/happy.fxh.git
//---white noise---------
float Hash11(float p)
{
    p = frac(p * .1031f);
    p *= p + 33.33f;
    p *= p + p;
    return frac(p);
}

float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * .1031f);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float Hash13(float3 p3)
{
    p3 = frac(p3 * .1031f);
    p3 += dot(p3, p3.zyx + 31.32f);
    return frac((p3.x + p3.y) * p3.z);
}

//http://burtleburtle.net/bob/hash/integer.html
//https://gist.github.com/badboy/6267743
//the follow function used by spatial-hash
uint HashJenkins32(uint a)
{
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);

    return a;
}

uint Hash64Shift32(uint64_t key)
{
    key = (~key) + (key << 18); // key = (key << 18) - key - 1;
    key = key ^ (key >>> 31);
    key = key * 21; // key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >>> 11);
    key = key + (key << 6);
    key = key ^ (key >>> 22);
    return (int) key;
}

/* split mix: is a fast, splittable pseudorandom number generator (PRNG)
* often used to seed stronger PRNGs (like XORShift or PCG) or 
* as a quick hash for non-adversarial data.
*/
uint64_t SplitMix64(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ULL; //Golden ratio-derived constant
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    x ^= (x >> 31);
    return x;
}


/*---IGN(interleaved gradient noise) noise----------
 from "Next Generation Post Processing inCall of Duty Advanced Warfare":
 Experimenting and optimizing a noise generator, we found a noise function that we could classify 
 as being half way between dithered and random, and that we called Interleaved Gradient Noise
 also: https://blog.demofox.org/2022/01/01/interleaved-gradient-noise-a-different-kind-of-low-discrepancy-sequence/
*/
float IGN(float2 p, int frame)
{
    p += float(frame) * 5.588238f;
    return frac(52.9829189f * frac(0.06711056f * float(p.x) + 0.00583715f * float(p.y)));
}
//----blue noise--------
//you can get free blue noise from here:http://momentsingraphics.de/BlueNoise.html
//blue noise usually sample one pre-generated texture at runtime, sample blue noise should use nearest filter 
//further reading: https://blog.demofox.org/2020/07/14/blue-noise-textures-for-dithering-and-randomization/
Texture3D<float3> blue_noise_texture;
SamplerState blue_noise_sampler;
float BlueNoise1D(float2 p, int frame)
{
    float3 warp_location = float3(p, frame);
    uint3 texture_coord = uint3(warp_location.x, , 0);
    return blue_noise_texture.Sample(blue_noise_sampler, texture_coord).x;
}

float2 BlueNoise2D(float2 p, int frame)
{
    float3 warp_location = float3(p, frame);
    uint3 texture_coord = uint3(warp_location.x, , 0);
    return blue_noise_texture.Sample(blue_noise_sampler, warp_location).xy;
}

//http://www.jcgt.org/published/0009/03/02/
//(N → N): pcg3d, pcg4d is recommend by above paper
//also:https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/ && https://rene.ruhr/gfx/gpuhash/
//https://aras-p.info/blog/2025/06/13/Voronoi-Hashing-and-OSL/
//modern cpu/gpu multiply is more cheaper，so can use pcg etc “Hash functions from several decades ago 
//were built on assumption that multiplication is very expensive, which is very much not the case anymore.”
uint3 pcg3d(uint3 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v ˆ = v >> 16u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    return v;
}

uint4 pcg4d(uint4 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.w;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.w += v.y * v.z;
    v ˆ = v >> 16u;
    v.x += v.y * v.w;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.w += v.y * v.z;
    return v;
}

//https://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
//https://nullprogram.com/blog/2018/07/31/
//https://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
//https://www.youtube.com/watch?v=SePDzis8HqY
//https://blog.demofox.org/2024/09/02/summing-blue-noise-octaves-like-perlin-noise/
//dot noise https://mini.gmshaders.com/p/dot-noise

float Pow3(float x)
{
    return x * x * x;
}

float2 Pow3(float2 x)
{
    return x * x * x;
}

float3 Pow3(float3 x)
{
    return x * x * x;
}

float4 Pow3(float4 x)
{
    return x * x * x;
}

float Pow4(float x)
{
    float xx = x * x;
    return xx * xx;
}

float2 Pow4(float2 x)
{
    float2 xx = x * x;
    return xx * xx;
}

float3 Pow4(float3 x)
{
    float3 xx = x * x;
    return xx * xx;
}

float4 Pow4(float4 x)
{
    float4 xx = x * x;
    return xx * xx;
}

float Pow5(float x)
{
    float xx = x * x;
    return xx * xx * x;
}

float2 Pow5(float2 x)
{
    float2 xx = x * x;
    return xx * xx * x;
}

float3 Pow5(float3 x)
{
    float3 xx = x * x;
    return xx * xx * x;
}

float4 Pow5(float4 x)
{
    float4 xx = x * x;
    return xx * xx * x;
}

// ======================================================================
// Float Vector Comparisons
// ======================================================================

// lessThan
bool2 lessThan(float2 a, float2 b)
{
    return bool2(a.x < b.x, a.y < b.y);
}
bool3 lessThan(float3 a, float3 b)
{
    return bool3(a.x < b.x, a.y < b.y, a.z < b.z);
}
bool4 lessThan(float4 a, float4 b)
{
    return bool4(a.x < b.x, a.y < b.y, a.z < b.z, a.w < b.w);
}

// greaterThan
bool2 greaterThan(float2 a, float2 b)
{
    return bool2(a.x > b.x, a.y > b.y);
}
bool3 greaterThan(float3 a, float3 b)
{
    return bool3(a.x > b.x, a.y > b.y, a.z > b.z);
}
bool4 greaterThan(float4 a, float4 b)
{
    return bool4(a.x > b.x, a.y > b.y, a.z > b.z, a.w > b.w);
}

// equal
bool2 equal(float2 a, float2 b)
{
    return bool2(a.x == b.x, a.y == b.y);
}
bool3 equal(float3 a, float3 b)
{
    return bool3(a.x == b.x, a.y == b.y, a.z == b.z);
}
bool4 equal(float4 a, float4 b)
{
    return bool4(a.x == b.x, a.y == b.y, a.z == b.z, a.w == b.w);
}

// notEqual
bool2 notEqual(float2 a, float2 b)
{
    return bool2(a.x != b.x, a.y != b.y);
}
bool3 notEqual(float3 a, float3 b)
{
    return bool3(a.x != b.x, a.y != b.y, a.z != b.z);
}
bool4 notEqual(float4 a, float4 b)
{
    return bool4(a.x != b.x, a.y != b.y, a.z != b.z, a.w != b.w);
}

// lessThanEqual
bool2 lessThanEqual(float2 a, float2 b)
{
    return bool2(a.x <= b.x, a.y <= b.y);
}
bool3 lessThanEqual(float3 a, float3 b)
{
    return bool3(a.x <= b.x, a.y <= b.y, a.z <= b.z);
}
bool4 lessThanEqual(float4 a, float4 b)
{
    return bool4(a.x <= b.x, a.y <= b.y, a.z <= b.z, a.w <= b.w);
}

// greaterThanEqual
bool2 greaterThanEqual(float2 a, float2 b)
{
    return bool2(a.x >= b.x, a.y >= b.y);
}
bool3 greaterThanEqual(float3 a, float3 b)
{
    return bool3(a.x >= b.x, a.y >= b.y, a.z >= b.z);
}
bool4 greaterThanEqual(float4 a, float4 b)
{
    return bool4(a.x >= b.x, a.y >= b.y, a.z >= b.z, a.w >= b.w);
}

// ======================================================================
// Signed Integer Vector Comparisons (int)
// ======================================================================

// lessThan
bool2 lessThan(int2 a, int2 b)
{
    return bool2(a.x < b.x, a.y < b.y);
}
bool3 lessThan(int3 a, int3 b)
{
    return bool3(a.x < b.x, a.y < b.y, a.z < b.z);
}
bool4 lessThan(int4 a, int4 b)
{
    return bool4(a.x < b.x, a.y < b.y, a.z < b.z, a.w < b.w);
}

// greaterThan
bool2 greaterThan(int2 a, int2 b)
{
    return bool2(a.x > b.x, a.y > b.y);
}
bool3 greaterThan(int3 a, int3 b)
{
    return bool3(a.x > b.x, a.y > b.y, a.z > b.z);
}
bool4 greaterThan(int4 a, int4 b)
{
    return bool4(a.x > b.x, a.y > b.y, a.z > b.z, a.w > b.w);
}

// equal
bool2 equal(int2 a, int2 b)
{
    return bool2(a.x == b.x, a.y == b.y);
}
bool3 equal(int3 a, int3 b)
{
    return bool3(a.x == b.x, a.y == b.y, a.z == b.z);
}
bool4 equal(int4 a, int4 b)
{
    return bool4(a.x == b.x, a.y == b.y, a.z == b.z, a.w == b.w);
}

// notEqual
bool2 notEqual(int2 a, int2 b)
{
    return bool2(a.x != b.x, a.y != b.y);
}
bool3 notEqual(int3 a, int3 b)
{
    return bool3(a.x != b.x, a.y != b.y, a.z != b.z);
}
bool4 notEqual(int4 a, int4 b)
{
    return bool4(a.x != b.x, a.y != b.y, a.z != b.z, a.w != b.w);
}

// lessThanEqual
bool2 lessThanEqual(int2 a, int2 b)
{
    return bool2(a.x <= b.x, a.y <= b.y);
}
bool3 lessThanEqual(int3 a, int3 b)
{
    return bool3(a.x <= b.x, a.y <= b.y, a.z <= b.z);
}
bool4 lessThanEqual(int4 a, int4 b)
{
    return bool4(a.x <= b.x, a.y <= b.y, a.z <= b.z, a.w <= b.w);
}

// greaterThanEqual
bool2 greaterThanEqual(int2 a, int2 b)
{
    return bool2(a.x >= b.x, a.y >= b.y);
}
bool3 greaterThanEqual(int3 a, int3 b)
{
    return bool3(a.x >= b.x, a.y >= b.y, a.z >= b.z);
}
bool4 greaterThanEqual(int4 a, int4 b)
{
    return bool4(a.x >= b.x, a.y >= b.y, a.z >= b.z, a.w >= b.w);
}

// ======================================================================
// Unsigned Integer Vector Comparisons (uint)
// ======================================================================

// lessThan
bool2 lessThan(uint2 a, uint2 b)
{
    return bool2(a.x < b.x, a.y < b.y);
}
bool3 lessThan(uint3 a, uint3 b)
{
    return bool3(a.x < b.x, a.y < b.y, a.z < b.z);
}
bool4 lessThan(uint4 a, uint4 b)
{
    return bool4(a.x < b.x, a.y < b.y, a.z < b.z, a.w < b.w);
}

// greaterThan
bool2 greaterThan(uint2 a, uint2 b)
{
    return bool2(a.x > b.x, a.y > b.y);
}
bool3 greaterThan(uint3 a, uint3 b)
{
    return bool3(a.x > b.x, a.y > b.y, a.z > b.z);
}
bool4 greaterThan(uint4 a, uint4 b)
{
    return bool4(a.x > b.x, a.y > b.y, a.z > b.z, a.w > b.w);
}

// equal
bool2 equal(uint2 a, uint2 b)
{
    return bool2(a.x == b.x, a.y == b.y);
}
bool3 equal(uint3 a, uint3 b)
{
    return bool3(a.x == b.x, a.y == b.y, a.z == b.z);
}
bool4 equal(uint4 a, uint4 b)
{
    return bool4(a.x == b.x, a.y == b.y, a.z == b.z, a.w == b.w);
}

// notEqual
bool2 notEqual(uint2 a, uint2 b)
{
    return bool2(a.x != b.x, a.y != b.y);
}
bool3 notEqual(uint3 a, uint3 b)
{
    return bool3(a.x != b.x, a.y != b.y, a.z != b.z);
}
bool4 notEqual(uint4 a, uint4 b)
{
    return bool4(a.x != b.x, a.y != b.y, a.z != b.z, a.w != b.w);
}

// lessThanEqual
bool2 lessThanEqual(uint2 a, uint2 b)
{
    return bool2(a.x <= b.x, a.y <= b.y);
}
bool3 lessThanEqual(uint3 a, uint3 b)
{
    return bool3(a.x <= b.x, a.y <= b.y, a.z <= b.z);
}
bool4 lessThanEqual(uint4 a, uint4 b)
{
    return bool4(a.x <= b.x, a.y <= b.y, a.z <= b.z, a.w <= b.w);
}

// greaterThanEqual
bool2 greaterThanEqual(uint2 a, uint2 b)
{
    return bool2(a.x >= b.x, a.y >= b.y);
}
bool3 greaterThanEqual(uint3 a, uint3 b)
{
    return bool3(a.x >= b.x, a.y >= b.y, a.z >= b.z);
}
bool4 greaterThanEqual(uint4 a, uint4 b)
{
    return bool4(a.x >= b.x, a.y >= b.y, a.z >= b.z, a.w >= b.w);
}

// ======================================================================
// Scalar Implementations (All Types)
// ======================================================================

// Float
bool lessThan(float a, float b)
{
    return a < b;
}
bool greaterThan(float a, float b)
{
    return a > b;
}
bool equal(float a, float b)
{
    return a == b;
}
bool notEqual(float a, float b)
{
    return a != b;
}
bool lessThanEqual(float a, float b)
{
    return a <= b;
}
bool greaterThanEqual(float a, float b)
{
    return a >= b;
}

// Int
bool lessThan(int a, int b)
{
    return a < b;
}
bool greaterThan(int a, int b)
{
    return a > b;
}
bool equal(int a, int b)
{
    return a == b;
}
bool notEqual(int a, int b)
{
    return a != b;
}
bool lessThanEqual(int a, int b)
{
    return a <= b;
}
bool greaterThanEqual(int a, int b)
{
    return a >= b;
}

// Uint
bool lessThan(uint a, uint b)
{
    return a < b;
}
bool greaterThan(uint a, uint b)
{
    return a > b;
}
bool equal(uint a, uint b)
{
    return a == b;
}
bool notEqual(uint a, uint b)
{
    return a != b;
}
bool lessThanEqual(uint a, uint b)
{
    return a <= b;
}
bool greaterThanEqual(uint a, uint b)
{
    return a >= b;
}

#include "Platform.hlsli"

#if !COMPILER_SUPPORT_MINMAX3
template<typename T>
inline T max3(T x, T y, T z)
{
    return max(max(x, y), z);
}

template<typename T>
inline T min3(T x, T y, T z)
{
    return min(min(x, y), z);
}
#endif

//find the n-th nonzero bit for uint
uint FindNthSetBit(uint mask, uint n)
{
    int last = countbits(mask) - n - 1;
    uint index = 16u;
    index += countbits(mask >> index) <= last ? -8 : 8;
    index += countbits(mask >> index) <= last ? -4 : 4;
    index += countbits(mask >> index) <= last ? -2 : 2;
    index += countbits(mask >> index) <= last ? -1 : 1;
    index -= countbits(mask >> index) == last;
    
    return index;
}

//find the n-th nonzero bit for uint2
uint FindNthSetBit(uint2 mask, uint n)
{
    uint index = 0u;
    uint low_non_zeros = countbits(mask.x);
    uint mask32 = 0u;
    
    if (low_non_zeros < n)
    {
        index = 32u;
        mask32 = mask.y;
        n -= low_non_zeros;
    }
    else
    {
        mask32 = mask.x;
    }
    
    return index + FindNthSetBit(mask32, n);   
}
//"Building an Orthonormal Basis from a 3D Unit Vector Without Normalization"
void GetHemiOrthoBasisFrisvad(float3 n, inout float3 b1, inout float3 b2)
{
    BRANCH
    if(n.z < -0.9999999f) //handle the singularity
    {
        b1 = float3(0.f, -1.f, 0.f);
        b2 = float3(-1.f, 0.f, 0.f);
    }
    else
    {
        const float a = 1.f / (1.f + n.z);
        const float b = -n.x * n.y * a;
        b1 = float3(1.f - n.x * n.x * a, b, -n.z);
        b2 = float3(b, 1.f - n.y * n.y * a, -n.y);
    }
}

//liu method, ”Building an Orthonormal Basis, Revisited“， the paper claims this is the most robust & performance method
//this method of constructing a basis from a vec3 is also used by `glam::Vec3::any_orthonormal_pair'
void GetHemiOrthoBasisLiu(float3 n, inout float3 b1, inout float3 b2)
{
    float sign = n.z >= 0.f ? 1.f : -1.f;
    const float a = -1.f / (sign + n.z);
    const float b = n.x * n.y * a;
    b1 = float3(1.f + sign * n.x * n.x * a, sign * b, -sign * n.x);
    b2 = float3(b, sign + n.y * n.y * a, -n.y);
}

//montecarlo sampling related functions
//fast equal-area mapping of sphere
//alogrithm "Fast Equal-Area Mapping of the (Hemi) Sphere using SIMD" 
float3 FastEqualAreaSphereMapping(float2 UV) //square-to-sphere transform
{
#if 0
    UV = 2 * UV - 1;
    float distance = 1 - (abs(UV.x) + abs(UV.y));
    float radius = 1 - abs(distance);
    float phi = radius == 0u ? 0u : (M_PI / 4) * ((abs(UV.y) - abs(UV.x)) / radius + 1u);
    float f = radius * sqrt(2 - radius * radius);
    return float3(f * sign(UV.x) * abs(cos(phi)), f * sign(UV.y) * abs(sin(phi)), sign(distance) * (1 - radius * radius));
#else
    //approximation of sin（x*pi/4) and cos(x*pi/4) x=[0, 2]
    const float t1 = 0.785398163f;
    const float t2 = -0.8074534e-1f;
    const float t3 = 0.24870958e-2f;
    const float t4 = -0.36522e-4f;
    const float t5 = 0.999993f;
    const float t6 = -0.308371f;
    const float t7 = 0.157863e-1f;
    const float t8 = -0.298371e-3f;
    
    UV = 2 * UV - 1;
    float distance = 1 - (abs(UV.x) + abs(UV.y));
    float radius = 1 - abs(distance);
    float f = radius * sqrt(2 - radius * radius);
    float phi = radius == 0u ? 0u : ((abs(UV.y) - abs(UV.x)) / radius + 1u); //to do debug... how to ensure phi=[0, M_PI/2] FIXME
    
    //approx sin(x*pi/4) ~= t1*x + t2*x^3 + t3*x^5 + t4*x^7
    //approx cos(x*pi/4) ~= t5 + t6*x^2 + t7*x^4 + t8*x^6
    return normalize(float3(f * sign(UV.x) * abs(t5 + t6 * phi * phi + t7 * Pow4(phi) + t8 * Pow5(phi) * phi),
                  f * sign(UV.y) * abs(t1 * phi + t2 * Pow3(phi) + t3 * Pow5(phi) + t4 * Pow5(phi) * phi * phi),
                  sign(distance) * (1 - radius * radius)));
#endif
}

//fast equal-area mapping of sphere
//alogrithm "Fast Equal-Area Mapping of the (Hemi) Sphere using SIMD"
float2 FastEqualAreaShpereInverseMapping(float3 direction) //sphere-to-square transform
{
    //approximation of atan(x)*2/pi, x=[0,1]
    const float t1 = 0.406758566246788489601959989e-5f;
    const float t2 = 0.636226545274016134946890922156f;
    const float t3 = 0.61572017898280213493197203466e-2f;
    const float t4 = -0.247333733281268944196501420480f;
    const float t5 = 0.881770664775316294736387951347e-1f;
    const float t6 = 0.419038818029165735901852432784e-1f;
    const float t7 = -0.251390972343483509333252996350e-1f;
    
    direction = normalize(direction);
    float3 abs_dir = abs(direction);
    float radius = sqrt(1 - abs_dir.z);
    float x = min(abs_dir.x, abs_dir.y) / (max(abs_dir.x, abs_dir.y) + M_EPSILON);
    
    //t1 + t2*a + t3*a^2 + t4*a^3 t5*a^4 + t6*a^5 + t7*a^6
    float phi = t6 + t7 * x;
    phi = phi * x + t5;
    phi = phi * x + t4;
    phi = phi * x + t3;
    phi = phi * x + t2;
    phi = phi * x + t1;
    
    phi = (abs_dir.x < abs_dir.y) ? 1 - phi : phi;
    float2 UV = float2(radius - phi * radius, phi * radius);
    UV = (direction.z < 0u) ? 1 - UV.yx : UV;
    UV = asfloat(asuint(UV) ^ (asuint(direction.xy) & 0x80000000u));
    
    return UV * 0.5 + 0.5;
}

//Octahedral Normal Vectors and unit vector encoding and decoding 
//algorithm "A Survey of Efficient Representations for Independent UnitVectors"
//the oct and spherical representations are clearly the most practical
//Low-frequency normal maps benefit from spatial compression in formats like
//DXN, and the additional gain from switching to oct is likely to be modest at best.
//For high-frequency normal maps, it would be interesting to explore the use of oct as
//a compressed format, since DXN will produce large error
float2 OctSignNotZero(float2 v)
{
    return float2((v.x >= 0.f) ? +1.f : -1.f, (v.y >= 0.f) ? +1.f : -1.f);
}

//https://shaderbits.com/blog/octahedral-impostors
//https://gpuopen.com/learn/fetching-from-cubes-and-octahedrons/
//the unreal engine 4 imposter implementation uses a similar mapping
//to place virtual cameras on an octahedron surrounding the object
float2 OctMapping(float3 v)
{
    //project the sphere onto the octahedron, and then onto the xy plane
    float2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
    //reflect the folds of the lower hemisphere over the diagonals
    return (v.z <= 0.0f) ? ((1.0f - abs(p.yx)) * OctSignNotZero(p)) : p;
}

float3 OctInverseMapping(float2 e)
{
    float3 v = float3(e.xy, 1.f - abs(e.x) - abs(e.y));
    v.xy = v.z < 0.f ? (1.0 - abs(v.yx)) * OctSignNotZero(v.xy) : v.xy;
    return normalize(v);
}

//test all neigh candidate to opt keep precise for bits
float2 OctMappingPrecise(float3 v, uint bits)
{
    float2 s = OctMapping(v);
    float M = float(1 << ((bits / 2) - 1)) - 1.f;
    s = floor(clamp(s, -1.f, +1.f) * M) * (1.f / M);
    float best_representation = s;
    float highest_cosine = dot(OctInverseMapping(s), v);
    //test all combinations of floor and ceil and keep the best.
    //note that at +/- 1, this will exit the square... but that
    //will be a worse encoding and never win.
    
    UNROLL
    for (uint i = 0; i <= 1; ++i)
    {
        for (uint j = 0; j <= 1; ++j)
        {
            if(i||j)
            {
                //Offset the bit pattern (which is stored in floating
                //point!) to FXRively change the rounding mode
                //(when i or j is 0: floor, when it is one: ceiling)
                float2 candidate = float2(i, j) * (1 / M) + s;
                float cosine = dot(OctInverseMapping(candidate), v);
                if(cosine > highest_cosine)
                {
                    highest_cosine = cosine;
                    best_representation = candidate;
                }
            }
        }

    }
    
    return best_representation;
}

//We note that rotating and scaling the standard oct encoding so that the +Z hemisphere
//covers the unit square in parameter space adds few operations and eliminates the branch 
//in the encoding function, while doubling the number of bit patterns used to encode the hemisphere
float2 HemiOctMapping(float3 v)
{
    //assume normalized input on +Z hemisphere.
    //project the hemisphere onto the hemi-octahedron,
    //and then into the xy plane
    float2 p = v.xy * (1.0f / (abs(v.x) + abs(v.y) + v.z));
    //rotate and scale the center diamond to the unit square
    return float2(p.x + p.y, p.x - p.y);
}

float3 HemiOctInverseMapping(float2 e)
{
    //rotate and scale the unit square back to the center diamond
    float2 temp = float2(e.x + e.y, e.x - e.y) * 0.5f;
    float3 v = float3(temp, 1.0 - abs(temp.x) - abs(temp.y));
    return normalize(v);
}

#endif //_MATH_INC_