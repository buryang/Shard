#pragma once

#include "Core/EngineGlobalParams.h"
#include "Utils/CommonUtils.h"

namespace Shard::Renderer
{
    namespace EAsyncFlags
    {
        enum
        {
            eUsePersistentClasAllocator = 0x1,
            eUseAsyncTransfer = 0x2,
            eUseDecoupledAsyncTransfer = 0x4,
        };
    }

    REGIST_PARAM_TYPE(UINT, SCENE_STREAM_ASYNC_FLAGS, EAsyncFlags::eUsePersistentClasAllocator);
    REGIST_PARAM_TYPE(UINT, SCENE_STREAM_MAX_LOAD_REQUESTS_PER_FRAME, 1024);
    REGIST_PARAM_TYPE(UINT, SCENE_STREAM_MAX_UNLOAD_REQUESTS_PER_FRAME, 4096);
    REGIST_PARAM_TYPE(UINT, SCENE_STREAM_MAX_TRANSFER_SIZE, 32); //32MB
    REGIST_PARAM_TYPE(UINT, SCENE_STREAM_MAX_GEOMETRY_SIZE, 1024 * 2); //1024*2 MB
    REGIST_PARAM_TYPE(UINT, SCENE_STREAM_MAX_CLAS_SIZE, 1024 * 2); //1024*2 MB
    //todo 

    struct GPUStreamingStats
    {
        uint32_t resident_groups_{ 0u };
        uint32_t resident_clusters_{ 0u };
        uint32_t max_groups_{ 0u };
        uint32_t max_clusters_{ 0u };

        uint32_t persistent_groups_{ 0u };
        uint32_t persistent_clusters_{ 0u };
        uint64_t persistent_data_bytes_{ 0u };
        uint64_t persistent_clas_bytes_{ 0u };

        uint64_t max_data_bytes_{ 0u };
        uint64_t reserved_data_bytes_{ 0u };
        uint64_t used_data_bytes_{ 0u };

    };

    class GPUSceneStreaming
    {
    public:
        void Init();
        void UnInit();
        void Reset();
        void PreRender(HAL::HALCommandContext& cmd_buffer);
        void PostRender(HAL::HALCommandContext& cmd_buffer);

        /** pre traversal scene*/
        void PreTraversal(HAL::HALCommandContext& cmd_buffer); //build nvidia clas accerlerate 
        void PostTraveral(HAL::HALCommandContext& cmd_buffer); //unload noused 
    private:
        size_type   persistent_geometry_size_{ 0u };
        size_type   operation_size_{ 0u };
        size_type   clas_operation_size_{ 0u };
        GPUStreamingStats   streaming_stat_;
    };
}
