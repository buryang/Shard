#include "BindlessResources.hlsli"

cbuffer probe_config
{
    uint src_index;
    uint dst_index;
};

[numthreads(1, 1, 1)]
void CSProbe()
{
    uint value = GetBindlessRWByteBufferUniform(src_index).Load(0u);
    GetBindlessRWByteBufferUniform(dst_index).Store(0u, value);
}
