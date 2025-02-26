#ifndef _MATH_INC_
#define _MATH_INC_

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

//code from "efficient coverage mask generation for antialiasing"
//@return unit angle 0-1 for -PI...PI
float Atan2Approx(float y, float x)
{
    return (y > 0) - sign(y) * (x / abs(x) + abs(y)) * 0.25f + 0.25f);
}

//&param unit angle 0-1 for -PI...PI
//&return approximates float2(sin(uint_angle * PI * 2 -PI), cos(uint_angle*PI * 2 - PI))
float2 SinCosApprox(float unit_angle)
{
    return normalize(float2(abs(abs(3 - 4 * unit_angle) - 2) - 1, 1 - abs(unit_angle * 4 - 2)));
}
#endif //_MATH_INC_