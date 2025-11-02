#pragma once

#include "Core/EngineGlobalParams.h"
#include "Utils/CommonUtils.h"
#include "SceneProxy.h"

namespace Shard::Renderer
{
    REGIST_PARAM_TYPE(UINT, SCENE_INSTANCE_CULLING_BATCH_SIZE, 256u); //number of instances per culling batch
    REGIST_PARAM_TYPE(UINT, SCENE_INSTANCE_CULLING_MAX_BATCHES_PER_FRAME, 1024u); //max number of culling batches per frame
    REGIST_PARAM_TYPE(BOOL, SCENE_INSTANCE_CULLING_MULTI_THREAD_ENABLED, true); //enable multi-threaded culling

    enum class EInstanceCullingMode
    {
        eDisable = 0,
        eCPUDriven = 1,
        eGPUDriven = 2,
     };

    //instance culling data structure for scene proxy
    struct SceneInstanceCullingReadBack
    {
        uint32_t    total_instances_{ 0u };
        uint32_t    visible_instances_{ 0u };
        uint32_t    culled_instances_{ 0u };
        //culling result bitmask, 1 bit per instance
        Render::RenderBuffer    instance_culling_mask_;
    };

    class InstanceCullingExecutor
    {
    public:
        void Init(Renderer::SceneProxy* scene_proxy);
        void UnInit();
        void PreRender(HAL::HALCommandContext& cmd_buffer);
        void PostRender(HAL::HALCommandContext& cmd_buffer);

        uint32_t AddCullingView(/*view struct*/);
        uint32_t GetCullingViewCount() const;
        ~InstanceCullingExecutor();
    private:
        DISALLOW_COPY_AND_ASSIGN(InstanceCullingExecutor);
    private:
        //instance culling result for each view
        SmallVector<SceneInstanceCullingReadBack>   culling_result_;
        Renderer::SceneProxy* scene_proxy_{ nullptr };
    };

    class VisiblePrimitivesCollector
    {
    public:
        friend class VisiblePrimitiveConsumer;

    };

    /**
     * \brief class consume visible primitive info from collector
     */
    class VisiblePrimitiveConsumer
    {
    public:
        virtual void AddBatch(const VisiblePrimitivesCollector& collector) = 0;
        virtual void BuildDrawCommand() = 0;
    };

}
