#include "FontsShaderCommon.hlsli"
#include "../BindlessResource.hlsli"

#ifdef FREE_TYPE_ALGO
FTVSOutput Main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
    FTVSOutput output;
    uint vid = instance_id * 4 + vertex_id;
    FontVertex vertex;
    output.pos = xx;
    output.uv = vertex.uv;
    return output;
}
#elif defined(GLYPH_OUTLINE_ALGO)
FTVSOutput Main(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
    
}
#endif