/*
* WARNING: ONLY FOR STATIC SCENE MESHES
* optimize spatial hashing RTAO from post: https://interplayoflight.wordpress.com/2025/11/23/spatial-hashing-for-raytraced-ambient-occlusion/
* improve hashing method with cuckoo hashing + stash and wave intrisics
*/

#include "../CommonUtils.hlsli"
#include "../BindlessResources.hlsli"

#define RTAO_MAIN_TBL_SIZE  (1<<20u) // 1048576 entries
#define RTAO_STASH_SIZE     (1<<14u) // 16384 entries ~1.56%
#define RTAO_CUCKOO_MAX_KICKOUTS 7u
#define RTAO_INVALID_INDEX  0xFFFFFFFFu 

struct SpatialHashRTAOParams
{
    uint hash_tbl1_rw_bf_index; //checksum zero=empty
    uint hash_tbl2_rw_bf_index; //checksum 
    uint payload_rw_bf_index; //.x = occlusion << 16 | sample count, .y = frame time
    uint stash_key_rw_bf_index;
    uint stash_index_rw_bf_index;
    uint frame_index; //todo other parameters
#ifdef SPATIAL_HASHING_RTAO_STAT
    uint stat_output_rw_bf_index;
#endif
    //screen resolution
    uint2 screen_resolution;
};

//pcg and xxhash32 from the original Spatial Hashing RTAO post
uint pcg(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

uint xxhash32(uint p)
{
    const uint P2 = 2246822519U, P3 = 3266489917U, P4 = 668265263U, P5 = 374761393U;
    uint h = p + P5;
    h = P4 * ((h << 17) | (h >> 15));
    h = P2 * (h ^ (h >> 15));
    h = P3 * (h ^ (h >> 13));
    return h ^ (h >> 16);
}

//two independent hash functions for cuckoo hashing
uint Hash1(uint k) { return k * 2654435761u; }                    // Knuth
uint Hash2(uint k) { return (k * 2246822519u) ^ 0x9E3779B1u; }

uint FindOrInsertCell( float3 world_position, float3 normal, float cell_size, out bool was_in_stash, out uint payload_index)
{
    was_in_stash = false;
    payload_index = RTAO_INVALID_INDEX;

    //step 1: compute cell key + checksum
    int3 ip = (int3)floor(world_position / cell_size);
    int3 inorm = int3(round(normal * 7.0f)); //quantize normal to 3 bits per channel
    float scale = cell_size * 10000.0f;

    uint key = pcg(asuint(scale) + pcg(ip.x + pcg(ip.y + pcg(ip.z + pcg(inorm.x + pcg(inorm.y + pcg(inorm.z))))));
    uint checksum = max(xxhash32(key), 1u); //checksum zero means empty

    uint hash1 = Hash1(key) & (RTAO_MAIN_TBL_SIZE - 1u);
    uint hash2 = Hash2(key) & (RTAO_MAIN_TBL_SIZE - 1u);

    //step 2: wave intrinsics to remove duplicates
    uint64_t match_mask = WaveMatch(key); //SM6.5+
    bool is_unique_in_wave = (match_mask == WaveGetLaneMask()); //only check self

    //if multi lane insert the same cell, only the first lane will do the insert
    uint final_payload_index = RTAO_INVALID_INDEX;
    if(WaveIsFirstLane() || is_unique_in_wave)
    {
        //step3: try to find in main table(cuckoo hashing)
        uint slot = RTAO_INVALID_INDEX;
        if(hash_tbl1[h1] == check_sum) slot = h1;
        else if(hash_tbl2[h1] == check_sum) slot = h1;
        else if(hash_tbl1[h2] == check_sum) slot = h2;
        else if(hash_tbl2[h2] == check_sum) slot = h2;

        //step 4 : search in stash (rare case)
        if(slot == RTAO_INVALID_INDEX)
        {
            UNROLL(4)
            for(uint i = 0; i < RTAO_STASH_SIZE; i += 64) //wave size
            {
                uint index = i + WaveGetLaneIndex();
                if(index >= RTAO_STASH_SIZE) break;
                if(stash_key_buf[index] == check_sum)
                {
                    slot = stash_index_buf[index];
                    was_in_stash = true;
                    break;
                }
            }

            if(slot != RTAO_INVALID_INDEX && WaveActiveAnyTrue(was_in_stash))
                was_in_stash = true;
        }

        //step 5: insert if not found（order empty slot first->cuckoo kickout->stash）
        if(slot == RTAO_INVALID_INDEX)
        {
            //try to observe empty slot
            uint ignored;
            if(hash_tbl1[h1] == 0) {}


            //kickout (at last 7 times)
            if(slot == RTAO_INVALID_INDEX)
            {
                uint cur_pos = h1;
                uint cur_checksum = checksum;
                uint cur_tbl = 0u; //0=tbl1, 1=tbl2

                bool kicked_out = false;
                for(uint k = 0u; k < RTAO_CUCKOO_MAX_KICKOUTS && !kicked_out; ++k)
                {
                    uint victim;
                    if(cur_tbl == 0u)
                    {
                        InterlockedExchange(hash_tbl1[cur_pos], cur_checksum, victim);
                    }
                    else
                    {
                        InterlockedExchange(hash_tbl2[cur_pos], cur_checksum, victim);
                    }

                    if(victim == 0)
                    {
                        slot = cur_pos; 
                        kicked_out = true;
                        break;
                    }

                    cur_checksum = victim;
                    cur_pos = (cur_tbl == 0u) ? (Hash2(key) & (RTAO_MAIN_TBL_SIZE - 1u)) : (Hash1(key) & (RTAO_MAIN_TBL_SIZE - 1u));
                    cur_tbl = 1u - cur_tbl;
                }

                //kickout failed, insert into stash
                if(!kicked_out)
                {
                    for(uint s = WaveGetLaneIndex(); s < RTAO_STASH_SIZE; s += WaveGetLaneCount())
                    {
                        InterlockedCompareExchange(stash_key_buf[s], 0u, checksum, ignored);
                        if(ignored == 0u){
                            stash_index_buf[s] = h1; //random choose h1 as payload index
                            slot = h1;
                            was_in_stash = true;
                            break;
                        }
                    }
                }
            }
        }

        //step 6: get payload index( main tbl using slot directly, stash align to h1)
        final_payload_index = (slot != RTAO_INVALID_INDEX) ? h1 : RTAO_INVALID_INDEX;
#ifdef SPATIAL_HASHING_RTAO_STAT
        if(was_in_stash) InterlockedAdd(stat_output_buf[1], 1u); //stash hits
        else if( slot != RTAO_INVALID_INDEX) InterlockedAdd(stat_output_buf[0], 1u); //main tbl hits
        else InterlockedAdd(stat_output_buf[2], 1u); //inserts
#endif
    }

    //step 7: broadcast payload index to all lanes
    final_payload_index = WaveReadFirstLane(final_payload_index);
    payload_index = final_payload_index;
    return payload_index;
}


float3 ScreenPosToWorldPos(uint2 screen_position, float depth)
{

}

float ComputeCellSize(float d, float f, float Ry, float sp, float smin)
{   
    float h = d * tan(f * 0.5);
    float sw = sp * (h * 2.0) / Ry;
 
    //From https://history.siggraph.org/wp-content/uploads/2022/08/2020-Talks-Gautron_Real-Time-Ray-Traced-Ambient-Occlusion-of-Complex-Scenes.pdf 
    //s_wd = 2^(floor(log2(sw / smin))) * smin
    float exponent = floor(log2(sw / smin));
    float swd = pow(2.0, exponent) * smin;
 
    return swd;
}

//one raytrace ao simulation
float TraceAO(float3 origin, float3 direction, float max_distance)
{

}


[numthreads(8, 8, 1)] //64 threads = 1 wave
void CSRTAO(uint3 dispatchID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint lane_index : SV_WaveLaneIndex)
{
    uint2 screen_pos = dispatchID.xy;
    if(any(screen_pos >= screen_resolution)) return;
    float depth = SampleBindlessTexture2D(depth_tex2d_index, sampler_index, (float2(screen_pos) + 0.5f) / float2(screen_resolution)).r;
    float3 world_pos = ScreenPosToWorldPos(screen_pos, depth);

    //todo read normal map if any from gbuffer
    float3 normal = ComputeNormalFromDepth(screen_pos, depth);

    //calculate cell size according to orginal post
    float dist = length(world_pos - camera_position);
    float cell_size = ComputeCellSize(); //max(0.02f, pow(2.0, round(log2(dist*0.5f))));
    //jitter world postion for reduce the denoising need
    //float2 rand2 = saturate(float2(rand01(rngState), rand01(rngState)));
    //rand2 = 2 * (rand2 - 0.5);
    //worldPos += jitter_scale * cell_size * (rand2.x * tangent + rand2.y * bitangent);

    bool was_in_stash;
    uint payload_index;

    uint index = FindOrInsertCell(world_pos, normal, cell_size, was_in_stash, payload_index);

    if(index != RTAO_INVALID_INDEX)
    {
        //trace ao
        float3 ao_origin = world_pos + normal * 0.01f;
        float ao = TraceAO(ao_origin, normal, ao_max_distance);

        //combine sample in wave
        uint payload = (uint(ao * 255.0f) << 16u) | 1u; //occlusion + sample count

        //reduce within wave
        for(uint offset = 32u; offset > 0u; offset >>= 1u)
        {
            payload += WaveActiveSum(payload & 0xffffu) << 0u | WaveActiveSum((payload >> 16u) & 0xffffu) << 16u;
        }

        if(WaveIsFirstLane())
        {
            uint2 prev_payload;
            InterlockedAdd(payload_buf[payload_index].x, payload & 0xffffu, prev_payload.x);
            payload_buf[payload_index].y = frame_index; //update frame time
        }
    }
}
    

