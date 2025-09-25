#include "../BindlessResources.hlsli"
#include "VSMCommon.hlsli"

#ifndef VSM_PAGE_DRAW_THREAD_GROUP_SIZE
#define VSM_PAGE_DRAW_THREAD_GROUP_SIZE VSM_DEFAULT_THREAD_GROUP_X
#endif

#ifndef VSM_ALLOC_COMMAND_SPACE_TREHAD_GROUP_SIZE
#define VSM_ALLOC_COMMAND_SPACE_TREHAD_GROUP_SIZE VSM_DEFAULT_THREAD_GROUP_X
#endif

#ifndef VSM_OUTPUT_COMMAND_TREHAD_GROUP_SIZE
#define VSM_OUTPUT_COMMAND_TREHAD_GROUP_SIZE VSM_DEFAULT_THREAD_GROUP_X
#endif

//minimum page size for large page 
#define VSM_LARGE_PAGE_MIN_AREA_SIZE 16u //4x4 pages

inline void OutputPageDrawCommand(uint page_index, VSMPerPageDrawCmd cmd)
{
    //todo 
}

inline uint2 GetPageCoordForThread(uint thread_index, uint page_table_width)
{
    return uint2(thread_index % page_table_width, thread_index / page_table_width);
}

struct VSMLagePageJob
{
    uint instance_id;
    uint mip_level;
    uint flags;
    uint4 page_rect; //in page coord
};

inline VSMLagePageJob WaveReadLaneAt(VSMLagePageJob job, uint lane_index)
{
    VSMLagePageJob result;
    result.instance_id = WaveReadLaneAt(job.instance_id, lane_index);
    result.mip_level = WaveReadLaneAt(job.mip_level, lane_index);
    result.flags = WaveReadLaneAt(job.flags, lane_index);
    result.page_rect = WaveReadLaneAt(job.page_rect, lane_index);
    return result;
}

[numthreads(VSM_PAGE_DRAW_THREAD_GROUP_SIZE, 1, 1)]
void CSPageDrawAnalyze( uint3 groupID : SV_GroupID, uint thread_index : SV_GroupIndex )
{
    const uint wave_size = WaveGetLaneCount();
    uint curr_lane_index = WaveGetLaneIndex();

    uint unwarp_groupID = 0;

    if(unwarp_groupID > 0u)
        return;

    //load culling work item
    

    for(uint primary_view_id = )
    {
        uint4 page_rect = 0;
        uint2 page_rect_size = page_rect.zw - page_rect.xy + 1;

        for(uint mip_level = )
        {

            bool is_do_large_page = false;
            VSMLagePageJob large_job = (VSMLagePageJob)0u;

            //add culling logic here
            is_do_large_page = page_rect_size.x * page_rect_size.y >= VSM_LARGE_PAGE_MIN_AREA_SIZE;
            
            large_job.instance_id = 0xffffffffu;
            large_job.mip_level = 0xffffffffu;
            large_job.flags = 0u;

            uint64_t large_page_mask = WaveActiveBallot(is_do_large_page);
            //distribute large page job across wave
            while(large_page_mask != 0u)
            {
                uint voted_lane_index = firstbitlow(large_page_mask);

                VSMLagePageJob curr_job = WaveReadLaneAt(large_job, voted_lane_index);
                for(uint index = curr_lane_index; index < wave_size; index += wave_size)
                {

                }

                large_page_mask &= ~(1ull << voted_lane_index);
            }

            if(!is_do_large_page)
            {
                for(uint y = page_rect.y; y < page_rect.w; ++y)
                {
                    for(uint x = page_rect.x; x < page_rect.z; ++x)
                    {
                        //mark
                    }
                }
            }
        }
    }
}

[numthreads(VSM_ALLOC_COMMAND_SPACE_TREHAD_GROUP_SIZE, 1, 1)]
void CSAllocateCommandInstanceOutputSpace(uint indirect_arg_index : SV_DispatchThreadID)
{
    
}

[numthreads(VSM_OUTPUT_COMMAND_TREHAD_GROUP_SIZE, 1, 1)]
void CSOutputPageDrawCommands(uint2 dispatch_threadID : SV_DispatchThreadID)
{
    
}