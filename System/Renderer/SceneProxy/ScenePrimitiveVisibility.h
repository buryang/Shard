#pragma once

#include "Core/EngineGlobalParams.h"
#include "Utils/CommonUtils.h"
#include "../RenderCommon.h"
#include "SceneProxy.h"
#include "SceneStreaming.h"

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

    //mesh pass type, for collecting pass-related mesh draw command on CPU
    enum class EMeshPassType
    {
        eBase,
        eDepth,
        //todo other types
        eNum,
    };

    enum class EIntersectType
    {
        eNone,
        ePartial,
        eFull,
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

    /**
     * \brief: helper class, using pre-baked blue noise volume to dither occlusion sampling pattern
     */
    class BlueNoiseOcclusionDither
    {
    protected:
        std::atomic<uint32_t> sample_counter_{ 0u };
        Render::TextureHandle noise_volume_;
    public:
        explicit BlueNoiseOcclusionDither(Render::TextureHandle noise_volume) :noise_volume_(noise_volume) {
            if (!noise_volume.IsValid()) {
                LOG(ERROR) << "BlueNoiseOcclusionDither: invalid noise volume texture handle";
            }
        }
        void Rest(){ sample_counter_.store(0u); }
        /*return fraction, multiply radius outside here*/
        float3 Dither() {
            /*Mimicking Unreal Engine's Streaming Mode */
            auto curr_counter = sample_counter_.fetch_add(1u);
            //assume noise volume is 64x64x64
            constexpr auto volume_size = 64u;
            constexpr auto sq_volume_size = volume_size * volume_size;
            uint3 tex_coord{
                curr_counter % volume_size,
                (curr_counter / volume_size) % volume_size,
                (curr_counter / sq_volume_size) % volume_size
            };
            return noise_volume_->Sample(tex_coord); //todo realize cpu 3d texture and sample logic
        }
    };

    /* do instancing culling work on GPU*/
    class InstanceCullingExecutor
    {
    public:
        explicit InstanceCullingExecutor(const Renderer::SceneProxy& scene_proxy) : scene_proxy_(scene_proxy) {}
        
        void Init();
        void UnInit();
        
        //instance culling works done before fxr module render
        void PreRender(Render::RenderGraph& graph);
        //instance culling works done after fxr module render
        void PostRender(Render::RenderGraph& graph);

        //append views that need culling work, batched culling, each batch represent a mesh pass draw command
        uint32_t AttachCullingView(SceneViewInstance& view);
        //sync and upload all view to GPU
        void FlushCullingViews();
        uint32_t GetCullingViewCount() const;
        ~InstanceCullingExecutor() { UnInit(); }
    private:
        DISALLOW_COPY_AND_ASSIGN(InstanceCullingExecutor);
    private:
        //instance culling result for each view
        SmallVector<SceneInstanceCullingReadBack>   culling_result_;
        const Renderer::SceneProxy& scene_proxy_;
    };

    struct PrimitiveMeta
    {
        uint32_t id_;
        uint32_t flags_;
    };

    static constexpr PrimitiveMeta null_primitive{ ~0u };
    using VisibleBuffer = TRingBuffer<PrimitiveMeta, null_primitive, Vector>;

    //MPSC primitive collector?
    class VisiblePrimitivesCollector
    {
    public:
        friend class VisiblePrimitiveConsumer;
    protected:
        VisibleBuffer primitive_buffer_; //ring buffer for primitve collector
    };

    using PrimitveArray = RenderArray<VisiblePrimitivesCollector>;

    /**
     * \brief class consume visible primitive info from collector
     */
    class VisiblePrimitiveConsumer
    {
    public:
        virtual void AddBatches(const VisiblePrimitivesCollector& collector);
        /*deal with all added batches */
        virtual void Flush() = 0;
        virtual bool IsBusy() const = 0;
        virtual ~VisiblePrimitiveConsumer() = default;
        auto GetCollectorCount() const { return primitive_collectors_.size(); }
    protected:
        PrimitveArray   primitive_collectors_;
    };

    /*
    * \brief add all collectors to streaming source
    */
    class VisiblePrimitiveStreamingConsumer final : public VisiblePrimitiveConsumer
    {
    public:
        explicit VisiblePrimitiveStreamingConsumer(GPUSceneStreaming& stream) :stream_(stream) {}
        void AddBatches(const VisiblePrimitivesCollector& collector) override;
        void Flush() override;
    protected:
        void BatchStreamingCommit(/*batch*/);
    private:
        GPUSceneStreaming&   stream_;
    };



    /*
    * \brief filter non-renderable primitves before render
    */
    class VisiblePrimitiveFilterConsumer final : public VisiblePrimitiveConsumer
    {
    public:
        VisiblePrimitiveFilterConsumer() = default;
    protected:
        xx;
    };

}
