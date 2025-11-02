#include "VTPageCommon.hlsli"

#ifdef VT_PAGE_INVALIDATE_CS
[numthreads(VT_COMPUTE_THREAD_GROUP_SIZE_X, 1, 1)]
void CSPhysXPageInvalidate( uint3 DTid : SV_DispatchThreadID )
{
}
#elif VT_PAGE_UPDATE_CS
[numthreads(VT_COMPUTE_THREAD_GROUP_SIZE_X, 1, 1)]
void CSPhysXPageUpdate()
{
	
}
#elif VT_PAGE_GENERATE_FLAGS_HMIPS_CS
/*build hierarchical page flags to make culling acceptable performance*/
[numthreads(VT_COMPUTE_THREAD_GROUP_SIZE_X, 1, 1)]
void CSGeneratePageFlagsHMips()
{
	
}
#elif VT_PAGE_PROPAGATE_MAPPED_HMIPS_CS
[numthreads(VT_COMPUTE_THREAD_GROUP_SIZE_X, 1, 1)]
void CSPropagateMappedHMips()
{
	
}
#elif VT_APPEND_PHYSX_PAGE_LIST_X
[numthreads(VT_COMPUTE_THREAD_GROUP_SIZE_X, 1, 1)]
void CSAppendPhysXPageList()
{
	
}
#endif

