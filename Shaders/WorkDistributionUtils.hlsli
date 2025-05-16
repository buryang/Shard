#ifndef _WORK_DISTRIBUTION_UITLS_
#define _WORK_DISTRIBUTION_UTILS_
#include "../CommonUtils.hlsli"
/**
* with an global corherent queue to redistribute new works for unbalance work
* like sw raytracing/traversal/tessellation which with the same format input payload
* maybe in the future use workgraphs instead.realize BQ algorithm in the follow paper
* https://arbook.icg.tugraz.at/schmalstieg/Schmalstieg_353.pdf
*/
#if SM6_PROFILE
#define USE_WORK_GRAPHS 1
#endif

#ifndef THREAD_GROUP_SIZE
#define THREAD_GROUP_SIZE 32
#endif

#if 1//!USE_WORK_GRAPHS
groupshared uint work_boundary[THREAD_GROUP_SIZE];

template<typename Task>
void DistributeWork(Task task, uint group_thread_index, uint num_work_items)
{
    const uint lane_count = WaveGetLaneCount();
    const uint lane_index = group_thread_index & (lane_count - 1);
    const uint queue_offset = group_thread_index & ~(lane_count - 1);
    
    int work_head = 0u;
    int work_tail = 0u;
    int source_head = 0u;
    
    uint work_source = lane_index;
    uint work_accum = 0u;
    
    if (WaveActiveAnyTrue(num_work_items != 0u))
    {
        //queue work
        uint first_work_item = WavePrefixSum(num_work_items);
        work_accum = first_work_item + num_work_items;
        work_tail = WaveReadLaneAt(work_accum, lane_count - 1);
        
        bool has_work = num_work_items != 0u;
        uint queue_index = WavePrefixCountBits(has_work);
        
        //compact
        if (WaveActiveAnyTrue(num_work_items == 0u))
        {
            if (has_work)
                work_boundary[queue_offset + queue_index] = lane_index;
            GroupMemoryBarrier();
            work_source = work_boundary[group_thread_index];
            work_accum = WaveReadLaneAt(work_accum, work_source);
            
            if (lane_index >= WaveActiveCountBits(has_work))
            {
                work_accum = work_tail + lane_count;
            }

        }
    }
        
    while(work_head < work_tail)
    {
        work_boundary[group_thread_index] = 0u;
        GroupMemoryBarrier();
        
        uint last_item_index = work_accum - work_head - 1;
        if(last_item_index < lane_count)
            work_boundary[queue_offset + last_item_index] = 1;
        GroupMemoryBarrier();
        
        bool is_boundary = work_boundary[group_thread_index];
        uint queue_index = source_head + WavePrefixCountBits(is_boundary);
        uint source_index = WaveReadLaneAt(work_source, queue_index);
        
        uint first_work_item = queue_index > 0u ? WaveReadLaneAt(work_accum, queue_index - 1) : 0u;
        uint local_item_index = work_head + lane_index - first_work_item;
        
        Task child_task = task.CreateChild(source_index);
        
        bool is_active = (work_head + lane_index < work_tail);
        child_task.RunChild(task, is_active, local_item_index);
        
        work_head += lane_count;
        source_head += WaveActiveCountBits(is_boundary);
    }
}

struct WorkQueueState
{
    uint read_offset;
    uint write_offset;
    uint task_count; //can temporarily be conservatively higher
};

#if 0 
 
struct OutputQueue
{
    RWByteAddressBuffer data_buffer;
    RWStructuredBuffer<WorkQueueState> state_buffer;
    uint state_index;
    uint size;
    
    uint Add()
    {
        uint write_offset;
        //todo
        return write_offset;
    }
    
    uint DataBufferLoad(uint address)
    {
        return data_buffer.Load(address);
    }
    
    uint4 DataBufferLoad4(uint address)
    {
        return data_buffer.Load4(address);
    }
    
    void DataBufferStore4(uint address, uint4 values)
    {
        data_buffer.Store4(address, values);
    }
    
    WorkQueueState GetState(uint index)
    {
        return state_buffer[index];
    }
};

struct InputQueue
{
    RWByteAddressBuffer data_buffer;
    RWStructuredBuffer<WorkQueueState> state_buffer;
    uint state_index;
    uint size;
    
    uint Remove()
    {
        uint read_offset;
        //todo
        return read_offset;
    }
    
    uint Num()
    {
        return state_buffer[state_index].write_offset;
    }
};

#endif

//u need define a static const instance of this structure 
struct GlobalWorkQueue
{
    RWByteAddressBuffer data_buffer;
    RWStructuredBuffer<WorkQueueState> state_buffer;
    uint state_index;
    uint size;
    
    uint Add() //return offset
    {
        uint write_count = WaveActiveAllTrue(true); //active lane count
        uint write_offset = 0u;
        if (WaveIsFirstLane())
        {
            InterlockedAdd(state_buffer[state_index].write_offset, write_count, write_offset);
            InterlockedAdd(state_buffer[state_index].task_count, write_count);
        }

        //return offset
        return WaveReadLaneFirst(write_offset) + WavePrefixCountBits(true);
    }
    
    void Remove()
    {
        uint read_offset;
        return read_offset;
    }
    
    void ReleaseTask()
    {
        
    }
    
    bool IsEmpty()
    {
        uint count;
        if (WaveIsFirstLane())
        {
            InterlockedAdd(state_buffer[state_index].task_count, 0, count);
        }
        return WaveReadLaneFirst(count) == 0u;
    }
    
    uint DataBufferLoad(uint address)
    {
        return data_buffer.Load(address);
    }
    
    uint4 DataBufferLoad4(uint address)
    {
        return data_buffer.Load4(address);
    }
    
    void DataBufferStore4(uint address, uint4 values)
    {
        data_buffer.Store4(address, values);
    }
    
    WorkQueueState GetState(uint index)
    {
        return state_buffer[index];
    }
};

template<typename Task>
void GlobalTaskLoopVariable(GlobalWorkQueue global_work_queue)
{
    bool is_task_completed = true;
    uint task_read_offset = 0u;
    
    while (true)
    {
        if (WaveActiveAllTrue(is_task_completed))
        {
            task_read_offset = global_work_queue.Remove();
            is_task_completed = task_read_offset >= global_work_queue.size;
            if (WaveActiveAllTrue(is_task_completed))
            {
                break;
            }

        }
        
        Task task = (Task) 0u;
        bool is_task_ready = false;
        if (!is_task_completed)
        {
            is_task_ready = task.Load(global_work_queue, task_read_offset);
        }
        
        if (WaveActiveAnyTrue(is_task_ready))
        {
            if (is_task_ready)
            {
                task.Run(global_work_queue);
                task.Clear(global_work_queue, task_read_offset);
                is_task_completed = true;
            }
        }
        else
        {
            if (global_work_queue.IsEmpty())
            {
                break;
            }
            else
            {
                DeviceMemoryBarrier();
                //ShaderYield();
            }

        }
    }
}


template<typename Task>
void GlobalTaskLoopVariable(GlobalWorkQueue global_work_queue, uint group_thread_index)
{
    bool is_task_completed = true;
    uint task_read_offset = 0u;
    
    while(true)
    {
        if( WaveActiveAllTrue(is_task_completed))
        {
            task_read_offset = global_work_queue.Remove();
            is_task_completed = task_read_offset >= global_work_queue.size;
            if(WaveActiveAllTrue(is_task_completed))
            {
                break;
            }

        }
        
        Task task = (Task) 0u;
        bool is_task_ready = false;
        if(!is_task_completed)
        {
            is_task_ready = task.Load(global_work_queue, task_read_offset);
        }
        
        if(WaveActiveAnyTrue(is_task_ready))
        {
            uint num_children = 0;
            if(is_task_ready)
            {
                num_children = task.Run();

            }
            
            DistributeWork(task, group_thread_index, num_children);
            if(is_task_ready)
            {
                task.Clear(global_work_queue, task_read_offset);
                is_task_completed = true;
            }
        }
        else
        {
            if (global_work_queue.IsEmpty())
            {
                break;
            }
            else
            {
                DeviceMemoryBarrier();
                //ShaderYield();
            }

        }
    } 
}
#else

#define INVALID_TASK_ID ~0u
#define INITIAL_TASK_COUNT 1024u //todo fixit code generated by deepseek

//---work graph structure definitions---
struct WorkPayload
{
    uint sourceID; //orignal work source identifier
    uint item_index; //local work item index
    uint depth; //hierarchy depth
    //Add custom payload fields here
};

struct TaskMetadata
{
    uint task_count;
    uint parentID;
};

//---root node for initial task generation---
[Shader("node")]
//[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
void WorkDistributionRoot(
    DispatchNodeInputRecord<void> Input,
    [MaxRecords(INITIAL_TASK_COUNT)] NodeOutput<WorkPayload> TaskOutput,
    [MaxRecords(1)] NodeOutput<TaskMetadata> MetadataOutput)
{
    // Initial task setup
    TaskMetadata meta;
    meta.task_count = INITIAL_TASK_COUNT;
    meta.parentID = INVALID_TASK_ID;
    MetadataOutput.Dispatch(meta);

    // Distribute initial work items
    for(uint i = 0; i < INITIAL_TASK_COUNT; i++)
    {
        FWorkPayload payload;
        payload.SourceID = i;
        payload.ItemIndex = 0;
        payload.Depth = 0;
        TaskOutput.Dispatch(payload);
    }
}

// --- Work Processing Node with Dynamic Task Generation ---
[Shader("node")]
[NodeLaunch("coalescing")]
void WorkProcessorNode(
    [MaxRecords(WAVE_SIZE * 2)] DispatchNodeInputRecord<WorkPayload> Input,
    [MaxRecords(WAVE_SIZE * 2)] NodeOutput<FWorkPayload> TaskOutput,
    [MaxRecords(1)] NodeOutput<FTaskMetadata> MetadataOutput)
{
    WorkPayload payload = Input.Get();
    
    // --- Your custom work processing logic ---
    DoWork(Context, payload.SourceID, payload.ItemIndex);

    // --- Dynamic Task Generation ---
    const uint laneCount = WaveGetLaneCount();
    const bool bShouldSpawn = /* Your spawn condition */;
    
    if(WaveActiveAnyTrue(bShouldSpawn))
    {
        uint newTasks = WaveActiveCountBits(bShouldSpawn);
        uint firstTask = WavePrefixCountBits(bShouldSpawn);
        
        if(bShouldSpawn)
        {
            FTaskMetadata meta;
            meta.TaskCount = newTasks;
            meta.ParentID = payload.SourceID;
            MetadataOutput.Dispatch(meta);

            FWorkPayload newPayload;
            newPayload.SourceID = payload.SourceID * laneCount + firstTask;
            newPayload.ItemIndex = payload.ItemIndex + 1;
            newPayload.Depth = payload.Depth + 1;
            TaskOutput.Dispatch(newPayload);
        }
    }
}

// --- Task Management Node ---
[Shader("node")]
[NodeLaunch("coalescing")]
void TaskManagerNode(
    [MaxRecords(1)] DispatchNodeInputRecord<FTaskMetadata> input,
    [MaxRecords(1)] NodeOutput<FTaskMetadata> output)
{
    TaskMetadata meta = input.Get();
    
    // Track task completion using wave operations
    uint activeTasks = WaveActiveCountBits(meta.task_count > 0);
    if(WaveIsFirstLane())
    {
        // Update global task counter
        InterlockedAdd(GlobalTaskCounter, activeTasks);
    }
    
    // Forward metadata if needed
    if(meta.TaskCount > 0)
    {
        Output.Dispatch(meta);
    }
}

// --- Work Graph Configuration ---
struct WorkDistributionGraph
{
    struct Root : DispatchNodeVOID {};
    struct Processor : DispatchNode<FWorkPayload> {};
    struct Manager : DispatchNode<FTaskMetadata> {};

    static const NodeID RootNode = 0;
    static const NodeID ProcessorNode = 1;
    static const NodeID ManagerNode = 2;

    static const NodeLinks[] = {
        {RootNode, ProcessorNode},
        {ProcessorNode, ManagerNode},
        {ManagerNode, ProcessorNode} // Feedback loop for task management
    };
};
#endif


#endif