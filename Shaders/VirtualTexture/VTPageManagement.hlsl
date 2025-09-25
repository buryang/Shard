#include "VTPageCommon.hlsli"

#ifdef VT_PAGE_INVALIDATE_CS
[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
}
#elif VT_PAGE_UPDATE_CS

#endif

