#pragma once
#include <string>
#include "Utils/CommonUtils.h"
#include "Render/RenderGraphBuilder.h"
#include "HAL/HALGlobalEntity.h"
#include "FXRModule/Base/FXRBaseModule.h"
#include "SceneProxy/ScenePrimitiveVisibility.h"

namespace Shard::Renderer
{
	enum ERenderPiplineType
	{
		eSprite,
		eHDRP_2D,
		eHDRP_3D,
		eDefault = eHDRP_3D,
	};

    struct ECSSpriteImageComponent
    {
        RenderTexture*    sprite_{ nullptr };
    };

    class RenderPipelineBase
    {
    public:
        /*view_render to pipeline is N:1, not 1:1 for need share information between view renders */
        virtual void Render(Span<FXR::ViewRender>& views) = 0;
        virtual ~RenderPipelineBase() = default;
    };

    class MINIT_API SpriteRenderPipeline2D : public RenderPipelineBase
    {
    public:
        void Render(Span<FXR::ViewRender>& views) override;
    };

    class HDRPFXRArray;

    //render pipeline for load 2d screen etc
    class MINIT_API HDRPRenderPipeline2D final : public RenderPipelineBase
    {
    public:
        void Render(Span<FXR::ViewRender>& views) override;
    };

    namespace FXR
    {
        class FXRShadowMapDraw;
    }

    class MINIT_API HDRPRender3D final : public RenderPipelineBase
    {
    public:
        //fixme register parameter anywhere like ue
        struct RenderCfg
        {
#ifdef RAY_TRACING
            struct RayTracingCfg
            {
                union {
                    struct {
                        uint32_t    use_rt_AO_ : 1;
                        uint32_t    use_rt_GI_ : 1;
                        uint32_t    use_rt_Occlusion_ : 1;
                    };
                    uint32_t    packed_bits_{ 0u };
                };

                //cpu culling config
                float    cull_maximum_minradius_;
                float    cull_frustum_angle_;
            };
#endif
            struct RasterCfg
            {
                union {
                    struct {

                        //cpu culling now
                        uint32_t    use_cpu_culling_ : 1;
                        uint32_t    use_soft_raster_ : 1;
                    };
                    uint32_t    packed_bits_{ 0u };
                };
                float    min_triangle_threshold_{};
                float    max_triangle_threshold_{};
                //todo
            };

            //render graph parameters
            union {
                struct {

                    uint32_t    use_immediate_mode_ : 1;
                    uint32_t    use_async_compute_ : 1;
                    uint32_t    use_custom_msaa_ : 1;

                    //render effect
                    uint32_t    with_velocity_rendered_ : 1;
                    uint32_t    with_volumetric_cloud_rendered_ : 1;
                    uint32_t    with_volumetric_fog_rendered_ : 1;
                    uint32_t    with_sky_atmosphere_rendered_ : 1;
                };
                uint32_t    packed_bits_{ 0u };
            };

            void Load(const std::string& path);
            void Dump(const std::string& path);
        };
        explicit HDRPRender3D(RenderCfg& cfg);
        const auto& Config()const;
    public:
        void Init();
        void UnInit();
        void Render(Span<FXR::ViewRender>& views) override;
    protected:
        //collect visibility primitives and inital mesh command
        void BeginInitViews(Span<FXR::ViewRender>& views);
        //collect light informations and upload to GPU
        void BeginBuildLights();
        //internal render 
        void RenderInternal(Render::RenderGraph& graph, Span<FXR::ViewRender>& views);
        void GatherMeshElements();//todo
        void UpdateSceneProxyFromECS();

        DISALLOW_COPY_AND_ASSIGN(HDRPRender3D);
    protected:
        Render::RenderGraph render_graph_;
        HDRPFXRArray        fxr_modules_;

        InstanceCullingExecutor culling_executor_; 

        JobHandle   sync_ecs_proxy_handle_; //update scene proxy from ecs
        JobHandle   init_views_handle_;
        JobHandle   gather_mesh_pass_handle_; 

        //frame counter
        uint64_t    frame_index_{ 0u };
    };

    /*a warpper of a fxr modules subarray, i group some fxr module 
     *together for better schedule
     */
    class HDRPFXRArray final
    {
    public:
        HDRPFXRArray() = default;
        explicit HDRPFXRArray(uint32_t capacity) {
            modules_.reserve(capacity);
        }
        void Draw(Render::RenderGraph& graph, Span<FXR::ViewRender>& views, ERenderPhase phase);
        bool IsEmpty() const { return modules_.empty(); }

        /*get the specified class obj*/
        template<class FXRDraw>
        FXRDraw& Get() {
            const auto hash = std::declval<FXRDraw>().GetTypeHash();
            auto iter = eastl::find(begin(), end(), [&hash](auto iter) { return  iter->GetTypeHash() == hash; });
            assert(iter != end());
            return **iter;
        }
        template<class FXRDraw>
        const FXRDraw& Get() const {
            const auto hash = std::declval<FXRDraw>().GetTypeHash();
            auto iter = eastl::find(cbegin(), cend(), [&hash](auto iter) { return  iter->GetTypeHash() == hash; });
            assert(iter != cend());
            return **iter;
        }
        //export internal modules
        FXR::FXRArray& operator()() {
            return modules_;
        }
        const FXR::FXRArray& operator()() const {
            return modules_;
        }
    protected:
        FXR::FXRArray   modules_;
    };

}
