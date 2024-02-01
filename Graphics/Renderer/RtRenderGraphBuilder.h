#pragma once

#include "Renderer/RtRenderPass.h"
#include "Renderer/RtRenderGraph.h"
#include "Renderer/RtRenderGraphExe.h"
#include "RHI/RHIGlobalEntity.h"

namespace Shard
{
    namespace Renderer
    {
        class RtRendererGraph;
        class MINIT_API RtRenderGraphBuilder
        { 
        public:
            struct BuildConfig
            {
                union
                {
                    struct
                    {
                        uint32_t    aync_enable_ : 1;
                        uint32_t    culling_passes_ : 1;
                        uint32_t    res_aliasing_enable_ : 1;
                        uint32_t    hw_raytrace_enable_ : 1;
                        uint32_t    barrier_split_ : 1;
                    };
                    uint32_t    cfg_bits_{ 0u };
                };
            };
            static void SetBuildConf(const BuildConfig& conf) {
                build_conf_ = conf;
            }
            void Compile();
            void Finalize();
            bool IsReady() const;
            FORCE_INLINE RtRendererGraph& GetRenderGraph() {
                return graph_;
            }
            FORCE_INLINE RtRenderGraphExecutor::SharedPtr GetRenderGraphExe() {
                return graph_exe_;
            }
        private:
            DISALLOW_COPY_AND_ASSIGN(RtRenderGraphBuilder);
            void CullingNoUsePasses();
            void AnalysisResourceUsage();
            //build resource barrier
            void AddResourceTransition();
        private:
            static BuildConfig  build_conf_;
            RtRendererGraph    graph_;
            RtRenderGraphExecutor::SharedPtr    graph_exe_;
        };
    }
}