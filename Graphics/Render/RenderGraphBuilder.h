#pragma once

#include "Core/EngineGlobalParams.h"
#include "Render/RenderPass.h"
#include "Render/RenderGraph.h"
#include "Render/RenderGraphExe.h"
#include "HAL/HALGlobalEntity.h"

#define RESORT_ORDER_PASS

namespace Shard
{
    namespace Render
    {
        REGIST_PARAM_TYPE(BOOL, RENDER_ASYNC_COMPUTE, false);
        REGIST_PARAM_TYPE(BOOL, RENDER_CULLING_PASS, false);
        REGIST_PARAM_TYPE(BOOL, RENDER_RESOURCE_ALIASING, false);
        REGIST_PARAM_TYPE(BOOL, RENDER_BARRIER_SPLIT, false);

        class RenderGraph;
        class MINIT_API RenderGraphBuilder
        { 
        public:
            static RenderGraphExecutor::SharedPtr Compile(RenderGraph& graph);
        private:
            void CullUnusedPasses(RenderResourceCache* cache);
            void GenerateOrderredPasses();
            void ReSortOrderredPasses();
            void MergeOrderedPasses();
            void InsertOrderedAutoResolvePasses();
            void AnalyseResourceUsage(RenderResourceCache* cache);
            void AnalyseResourceHALMemoryUsage(RenderResourceCache* cache);
        private:
            //build resource barrier
            void AnalyseResourceTrasientResidency(RenderResourceCache* cache);
            void AnalyseResourceTransition(RenderResourceCache* cache);
            explicit RenderGraphBuilder(RenderGraph& graph) :graph_(graph) {}
            DISALLOW_COPY_AND_ASSIGN(RenderGraphBuilder);
        private:
            RenderGraph&    graph_;
            Vector<RenderPass::Handle>  ordered_passes_;
        };
    }
}