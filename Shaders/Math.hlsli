#ifndef _MATH_INC_
#define _MATH_INC_

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

#endif //_MATH_INC_