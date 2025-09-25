#ifndef _CULLING_INC_
#define _CULLING_INC_

#include "../CommonUtils.hlsli"
#include "../Math.hlsli"
#include "Culling/PrimitiveCullingUtils.hlsli"
#include "ClusterCommon.hlsli"


#define CULLING_CLUSTER_WORKGROUP 64 //
#define CULLING_INSTANCE_WPO_ENABLED //whether need world position offset enabled 

//64bit-atomic: 32-bit Depth; 32-bit Triangle/Visible Cluster ID
struct VisibleID
{
    uint64 packed_value;
};

struct TraversalClusterCullingCallback
{
    void PreCulling()
    {
        
    }
    
    void ProcessCluster(Cluster cluster)
    {
    
    }
    
    void PostCulling()
    {
        
    }
};


#endif //_CULLING_INC_ 