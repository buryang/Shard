#ifndef _INSTSNCE_CULLING_SCHEDULE_INC_
#define _INSTSNCE_CULLING_SCHEDULE_INC_
#include "../CommonUtils.hlsli"
#include "InstanceCullingCommon.hlsli"

//the dispatch size for instance culling, should keep in sync with the cpu side define
#ifndef INSTANCE_CULLING_DISPATCH_GROUP_SIZE 
#define INSTANCE_CULLING_DISPATCH_GROUP_SIZE 128u
#endif 

#ifndef INSTANCE_CULLING_THREAD_GROUP_SIZE_X
#define INSTANCE_CULLING_THREAD_GROUP_SIZE_X 128u
#endif

struct InstanceCullingBatch
{
   uint instance_offset;
   uint instance_count; 
};

struct InstanceCullingTask 
{
    InstanceCullingBatch batch;
    uint flags;
};

inline uint PackInstanceCullingBatch(InstanceCullingBatch batch)
{
    return batch.instance_offset | (batch.instance_count << 16u);
}

inline void UnpackInstanceCullingBatch(uint packed, out InstanceCullingBatch batch)
{
    batch.instance_offset = packed & 0xffffu;
    batch.instance_count = (packed >> 16u) & 0xffffu;
}

InstanceCullingTask InstanceCullingTaskDistrite(uint3 groupID : SV_GroupID, uint thread_index : SV_GroupIndex, uint groups_per_batch)
{

    InstanceCullingTask task = (InstanceCullingTask)0;
    //todo

    return task; 
}

[numthreads(INSTANCE_CULLING_THREAD_GROUP_SIZE_X, 1, 1)]
void CSInstanceCullingBufferBuild(uint3 groupID : SV_GroupID, uint thread_index : SV_GroupIndex)
{
    uint batch_index = GetLinearUnwarpedGroupID(groupID, INSTANCE_CULLING_DISPATCH_GROUP_SIZE)；
}

#endif //_INSTSNCE_CULLING_SCHEDULE_INC_