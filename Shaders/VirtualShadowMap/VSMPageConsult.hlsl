#include "../VirtualTexture/VTPageCommon.hlsli"


#define VSM_MAX_MARKING_AREA_PER_THREAD 8u
#define VSM_MARKING_TASK_QUEUE_SIZE (VSM_MAX_MARKING_AREA_PER_THREAD * 2u)

#define VSM_PAGE_CONSULT_THREAD_GROUP_SIZE_X 64u

groupshared uint num_group_marking_tasks;
groupshared uint2 group_marking_tasks[VSM_MARKING_TASK_QUEUE_SIZE];


[numthreads(VSM_PAGE_CONSULT_THREAD_GROUP_SIZE_X, 1, 1)]
void CSVoteHZBPagesAndUpdateDirtyFlags( uint3 DTid : SV_DispatchThreadID )
{
}


[numthreads(VSM_PAGE_CONSULT_THREAD_GROUP_SIZE_X, 1, 1)]
void CSBuildPageHZB(uint3 dispatch_threadID:SV_DispatchThreadID)
{
	
}


[numthreads(VSM_PAGE_CONSULT_THREAD_GROUP_SIZE_X, 1, 1)]
void CSBuildPageHZBTop(uint3 dispatch_threadID:SV_DispatchThreadID)
{
	
}

