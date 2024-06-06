#pragma once

#include "Utils/Handle.h"
#include "Utils/Memory.h"
#include "Utils/CommonUtils.h"
#include "Core/EngineGlobalParams.h"
#include "HAL/HALGlobalEntity.h"
#include "RenderResources.h"
#include "RenderPass.h"


REGIST_PARAM_TYPE(UINT, RENDER_DEFRAG_PASS, 0);
REGIST_PARAM_TYPE(BOOL, RENDER_SPLIT_BARRIER, false);

namespace Shard
{
    namespace Render
    {
        //think about whether new rhi proxy
        class MINIT_API RenderGraphExecutor
        {
        public:
            using Ptr = RenderGraphExecutor*;
            using SharedPtr = std::shared_ptr<RenderGraphExecutor>;
            using CallBack = std::function<void(RenderGraphExecutor& executor)>;
        public:
            void Execute();
            //whether executor ready for executing, check bake status and resource status
            bool IsReady()const;
        private:
            friend class RenderGraphBuilder;
            void AllocateRenderResources();
            /*collection information of all memory, and do allocation*/
            void AnalyseRenderResourceResidency();
            /*analyse pass and resource synchronization*/
            void AnalyseRenderResourceSync();
            void ExecutePassPrologue(HAL::HALCommandContext* context, RenderPass::Handle pass);
            void ExecutePass(HAL::HALCommandContext* context, RenderPass::Handle pass);
            void ExecutePassEpilogue(HAL::HALCommandContext* context, RenderPass::Handle pass);
            DISALLOW_COPY_AND_ASSIGN(RenderGraphExecutor);
        private:
            friend class RenderGraphBuilder;
            std::unique_ptr<RenderResourceCache>    resource_cache_;
            Vector<RenderPass::Handle>  ordered_passes_;
            Vector<HAL::HALAllocation*> alias_allocations_;
        };

        //global render module allocator
        extern Utils::ScalablePoolAllocator<uint8_t>* g_render_allocator;

        template<class ShaderParameter, typename... Args>
        ShaderParameter* CreateShaderParameters(Args&&... args) {
            auto* ptr = reinterpret_cast<ShaderParameter*>g_render_allocator->allocate(sizeof(ShaderParameter));
            new(ptr)ShaderParameter(std::forward<Args>(args)...);
            return ptr;
        }

        template<class ShaderParameter>
        void DestroyShaderParameters(ShaderParameter* ptr) {
            g_render_allocator->deallocate((uint8_t*)ptr, sizeof(ShaderParameter));
        }
    }
}
