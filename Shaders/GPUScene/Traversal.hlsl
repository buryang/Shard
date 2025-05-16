#include "../CommonUtils.hlsli"
#include "../WorkDistributionUtils.hlsli"
#include "ClusterCommon.hlsli"

#define NODE_PACKED_IS_GROUP 0 : 1
#define NODE_PACKED_GROUP_INDEX 1 : 23
#define NODE_PACKED_CHILD_OFFSET 1 : 26
#define NODE_PACKED_CHILD_COUNT_MINUS_ONE 27 : 5

#define TRAVERSAL_INIT_WORKGROUP 128
#define TRAVERSAL_RUN_WORKGROUP 64

struct ClusterInfo
{
    uint instanceID;
    uint clusterID;
};

struct TraversalInfo
{
    uint instanceID;
    uint packed_node;
};

inline void UnpackTraversalInfo(uint2 packed_info, out TraversalInfo info)
{
    info.instanceID = packed_info.x;
    info.packed_node = packed_info.y;
}

uint SetupTask(inout TraversalInfo traveral_info, uint read_index, uint pass_index)
{
    uint sub_count = 0u;
    bool is_node = !UNPACK_BITS(traveral_info.packed_node, NODE_PACKED_IS_GROUP);
    if (is_node)
    {
        sub_count = UNPACK_BITS(traveral_info.packed_node, NODE_PACKED_CHILD_COUNT_MINUS_ONE);
    }
    else
    {
        sub_count = UNPACK_BITS(traveral_info.packed_node, );
    }
    
    return sub_count + 1;
}

#define MAX_TRAVERSAL_TASK_IN_WAVE 64

groupshared uint s_tasks[MAX_TRAVERSAL_TASK_IN_WAVE];

void ProcessSingleSubTask(inout TraversalInfo traversal_info, uint taskID, uint sub_taskID, bool valid, uint thrad_read_index, uint pass_index)
{
    
}


void ProcessAllSubTask(inout TraversalInfo traversal_info, bool thread_runnable, uint thread_sub_count, uint thread_read_index, uint pass_index)
{
    const uint wave_count = WaveGetLaneCount();
    uint offset_end = asuint(WavePrefixSum(asfloat(thread_sub_count)));
    uint offset_start = offset_end - thread_sub_count;
    uint total_threads = WaveReadLaneAt(offset_end, wave_count);
    //dispatch task in runs
    uint total_runs = (total_threads + wave_count - 1) / wave_count;
    const uint 
    
    bool has_task = total_threads > 0;
    uint task_count = WaveActiveCountBits(has_task);
    uint task_offset = WavePrefixCountBits(has_task);
    
    if (has_task)
    {
        s_tasks[]
    }
    
    AllMemoryBarrier();
    
    uint enqueues_count = 0u;
    
    int task_base = -1;
    for (uint r = 0; r < total_runs; ++r)
    {
        uint tfirst = r * wave_count;
        uint t = tfirst + WaveGetLaneIndex();
        
        int rel_start = offset_start - tfirst;
        uint start_bits = WaveActiveBitOr(thread_runnable && rel_start >= 0 && rel_start <= 32 ? (1 << rel_start) : 0u);
        int task = countbits(start_bits & ) + task_base;
        
        
        task_base = WaveReadLaneAt(task, wave_count - 1);//for next run
        //do single task
        ProcessSingleSubTask(traversal_info,);
    }

}

//initialization entry function, seed root's of visible instance
void CSnitMain()
{
    
}


//refresh running entry function, all renderable clusters are output
//with render_cluster_infos, and its counter
void CSRunMain()
{
    uint thread_read_index = ~0u;
    
    for (uint pass_index = 0;;++pass_index)
    {
        //if entire wave has no work, acquire new work
        if(WaveActiveAllTrue(thread_read_index == ~0u))
        {
            //pull new work
            if (WaveIsFirstLane())
            {
                InterlockedAdd(GLOBAL_COUNTER, SUB_GROUP_SIZE, thread_read_index);
            }
            
            thread_read_index = WaveReadLaneFirst(thread_read_index) + WaveGetLaneIndex();
            thread_read_index = thread_read_index >= MAX_TRAVERINFO ? ~0u : thread_read_index;

            //check whether all read out of bound agein
            if (WaveActiveAllTrue(thread_read_index == ~0u))
            {
                break;
            }
        }
        
        //let's attempt to fetch work
        bool thread_runnable = false;
        TraversalInfo node_traveral_info;
        
        while(true)
        {
            if(thread_read_index != ~0u)
            {
                AllMemoryBarrier(); //to fix it
                //unpack traversal info and invalidate it
                uint64 raw_value = xxx;
                thread_runnable = raw_value != ~0u;
            }
            
            if (WaveActiveAnyTrue(thread_runnable))
            {
                break;
            }
            
            //entire warp saw no valid work.
            //we always race ahead with reads compared to writes, but we may also
            //simply have no actual tasks left.
            AllMemoryBarrier();
            bool is_empty = false; //to do check 
            if(WaveActiveAllTrue(is_empty))
            {
                return;
            }

        }
        
        //some work to consume
        if(WaveActiveAnyTrue(thread_runnable))
        {
            uint thread_sub_count = 0;
            if (thread_runnable)
            {
                thread_sub_count = SetupTask(node_traveral_info, thread_read_index, pass_index);
            }
            
            ProcessAllSubTask();
            
            uint num_runable = WaveActiveCountBits(thread_runnable);
            
            if (WaveIsFirstLane())
            {
                InterlockedAdd(, -int(num_runable));
            }
            
            if (thread_runnable)
            {
                //invalid read index
                thread_read_index = ~0u;
            }

        }
    }
}


    