#pragma once
#include <string>
#include "Utils/CommonUtils.h"
#include "Render/RenderGraphBuilder.h"
#include "RuntimeSceneRenderer.h"
#include "RuntimeWorkIR.h"

namespace Shard::Render
{
    namespace Runtime
    {
        struct ECSSpriteImageComponent
        {
            Texture*    sprite_{ nullptr };
        };

        class MINIT_API RuntimeSpriteRender2D : public RuntimeRenderBase
        {
        public:
            void Render() override;
        };

        //render for load 2d screen etc
        class MINIT_API RuntimeHDRPRender2D final : public RuntimeSceneRenderBase
        {
        public:
        protected:
            void RenderScene() override;
        };

        class MINIT_API RuntimeHDRPRender3D final : public RuntimeSceneRenderBase
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

                        uint32_t                        use_immediate_mode_ : 1;
                        uint32_t                        use_async_compute_ : 1;
                        uint32_t                        use_custom_msaa_ : 1;

                        //render effect
                        uint32_t                        with_velocity_rendered_ : 1;
                        uint32_t                        with_volumetric_cloud_rendered_ : 1;
                        uint32_t                        with_volumetric_fog_rendered_ : 1;
                        uint32_t                        with_sky_atmosphere_rendered_ : 1;
                    };
                    uint32_t    packed_bits_{ 0u };
                };

                void Load(const std::string& path);
                void Dump(const std::string& path);
            };
            RuntimeHDRPRender3D(RenderCfg& cfg);
            const auto& Config()const;
        protected:
            void RenderScene() override;
        private:
            bool GatherDyamicMeshDrawCommand(Render::RenderGraphBuilder& builder);
            bool GatherRayTracingWorld(Render::RenderGraphBuilder& builder);
        private:
            RenderCfg    config_;
            Vector<DrawWorkCommandIR>    static_mesh_draws_;
        };

    }
}
