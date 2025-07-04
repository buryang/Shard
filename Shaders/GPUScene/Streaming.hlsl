#include "../GPUMem/MemoryArena.hlsli"
#include "GPUSceneCommon.hlsli"
#include "StreamingCommon.hlsli"

/**
* for more background information, reference to giga voxel chapter 7 "out-of-core data management"
*/

//defrag of the clas memory, memory compaction
void CSDefragMain(uint3 dispatch_threadID : SV_DispatchThreadID)
{
    uint global_thread_index = dispatch_threadID.x;
    
}

//filter out too old clas 
void CSAgeFilterMain(uint3 dispatch_threadID : SV_DispatchThreadID)
{
    
}