#include "../GPUMem/MemoryArena.hlsli"
#include "GPUSceneCommon.hlsli"
#include "StreamingCommon.hlsli"

//defrag of the clas memory
void CSDefragMain(uint3 dispatch_threadID : SV_DispatchThreadID)
{
    uint global_thread_index = dispatch_threadID.x;
    
}

//filter out too old clas 
void CSAgeFilterMain()
{
    
}