#ifndef _VT_DISPATCHER_INC_
#define _VT_DISPATCHER_INC_

struct VTPageDispatcherEntry
{
    uint2 page_offset_;
    uint mip_level_start_;
    uint num_mips_;
    uint id_;
    uint flags_;
};

template<typename DispatchFunc>
void DispatchPageEntry(in VTPageDispatcherEntry entry, uint3 dispatch_threadID:SV_DispatchThreadID)
{
    //
}

#endif //_VT_DISPATCHER_INC_