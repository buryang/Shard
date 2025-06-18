#ifndef _RAYTRACE_COMMON_INC_
#define _RAYTRACE_COMMON_INC_

#include "../Math.hlsli"

//returns true if the ray intersects the box within
//according to https://jcgt.org/published/0007/03/04/paper.pdf following is slab method
bool IntersectBox(float3 ray_orign, float3 ray_dir, float3 box_center, float3 box_half, inout float fly_time)
{
    //compute reciprocals of direction components
    float3 inv_dir = rcp(ray_dir);
    float3 local_box_center = box_center - ray_orign;
    
    //calculate intersection times for each axis
    float3 t0 = (local_box_center - box_half) * inv_dir;
    float3 t1 = (local_box_center + box_half) * inv_dir;
    
    //find the latest entry (tmin) and earliest exit (tmax)
    float3 min_time = min(t0, t1);
    float3 max_time = max(t0, t1);
    
    float max_min = max3(min_time.x, min_time.y, min_time.z);
    float min_max = min3(max_time.x, max_time.y, max_time.z);
    fly_time = max_min;
    
    return max_min < min_max;
}

//https://jcgt.org/published/0007/03/04/paper.pdf intersect box algorithm 
//this solution is designed for large numbers of boxes and oriented boxes, and it is
//generally lessefficient in the BVHAABB case. the paper says Axis-aligned box require normal 
//result or oriented-box this alg is fastest
bool IntersectBoxOrientedWithNormal(float3 ray_origin, float3 ray_dir, float3 box_center, float3 box_half, float3x3 box_to_world, const in bool oriented, const in bool can_start_in_box, inout float fly_time, inout float3 normal)
{
    ray_origin -= box_center;
    if (oriented)
    {
        ray_dir *= box_to_world;
        ray_origin *= box_to_world;
    }
    float inv_box_half = rcp(box_half);
    //The specific GLSL¡¡implementation given adjusts the order of operations in some non-obvious ways in
    //order to reduce the peakregister count.
    float winding = can_start_in_box && (max3(abs(ray_origin) * inv_box_half) < 1.0) ? -1f : 1f;
    float3 sign = -sign(ray_dir);
    float3 d = box_half * winding * sign - ray_origin;
    if(oriented)
    {
        d /= ray_dir;
    }
    else
    {
        d *= 1;
    }
    
    #define TEST(U, VW) (d.U >= 0.0) && all(lessThan(abs(ray_origin.VW + ray_dir.VW*d.U), box_half.VW))
    bool3 test = bool3(TEST(x, yz), TEST(y, zx), TEST(z, xy));
    sign = test.x ? float3(sign.x, 0, 0) : (test.y ? float3(0, sign.y, 0) : float3(0, 0, test.z ? sign.z : 0));
    #undef TEST
    
    fly_time = (sign.x != 0) ? d.x : ((sign.y != 0) ? d.y : d.z);
    normal = oriented ? (box_to_world * sign) : sign;
    return any(sign != 0);

}

//https://iquilezles.org/articles/intersectors/, only ray_origin outside of sphere
bool IntersectSphere(float3 ray_orign, float3 ray_dir, float3 sphere_center, float sphere_radius, inout float fly_time)
{
    float3 oc = ray_orign - sphere_center;
    float b = dot(oc, ray_dir);
    float3 qc = oc - b * ray_dir;
    float h = sphere_radius * sphere_radius - dot(qc, qc);
    if (h < 0)
    {
        return false;
    }
    
    h = sqrt(h);
    fly_time = -b - h;
    return true;
}

//plane degined by p (p.xyz be normalized)
bool IntersectPlane(float3 ray_origin, float3 ray_dir, float4 plane, out float fly_time)
{
    float p = dot(ray_dir, plane.xyz);
    if (p < M_EPSILON)
    {
        return false;
    }
    fly_time = -(dot(ray_origin, plane.xyz) + plane.w) / p;
    return true;
}

//code from https://www.shadertoy.com/view/4tsBD7
bool IntersectDisk(float3 ray_origin, float3 ray_dir, float3 disk_center, float3 disk_normal, float disk_radius, out float fly_time)
{
    float3 o = ray_origin - disk_center;
    float t = -dot(disk_normal, o) / dot(ray_dir, disk_normal);
    float3 q = o + ray_dir * t;
    if (dot(q, q) < disk_radius * disk_radius)
    {
        fly_time = t;
        return true;
    }
    return false;
}

struct Ray
{
    float3 origin;
    float3 dir;
    
    void Init(float3 ray_origin, float3 ray_dir)
    {
        origin = ray_origin;
        dir = ray_dir;
    }
};

struct DDA
{
    Ray ray;
    //todo other config
    void Init(in Ray ray, )
    {
        
    }
    
    void Step()
    {
        
    }
    
    void March(float distance)
    {
        
    }
};

#endif //_RAYTRACE_COMMON_INC_