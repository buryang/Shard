#pragma once

#include "Render/RenderPass.h"
#include "Render/RenderGraph.h"
#include "Render/RenderGraphExe.h"
#include "RHI/RHIGlobalEntity.h"

namespace Shard
{
    namespace Render
    {
        class RenderGraph;
        class MINIT_API RenderGraphBuilder
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
            bool IsReadyToBake() const;
            RenderGraphExecutor::SharedPtr Bake();
            FORCE_INLINE RenderGraph& GetRenderGraph() {
                return graph_;
            }
        private:
            DISALLOW_COPY_AND_ASSIGN(RenderGraphBuilder);
            void CullUnusedPasses();
            void GenerateOrderredPasses();
            void MergeOrderedPasses();
            void AddOrderredHelperPasses();
            void AnalyseResourceUsage();
            //build resource barrier
            void AddResourceTransition();
            void AllocateRHIResources(RenderGraphExecutor* exector);
        private:
            static BuildConfig  build_conf_;
            RenderGraph    graph_;
        };
    }
}