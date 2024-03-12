#pragma once

#include "Utils/Handle.h"
#include "Utils/Memory.h"
#include "Core/EngineGlobalParams.h"
#include "Graphics/RHI/RHIGlobalEntity.h"
#include "Graphics/Renderer/RtRenderResources.h"
#include "Graphics/Renderer/RtRenderPass.h"


REGIST_PARAM_TYPE(UINT, RENDER_DEFRAG_PASS, 0);

namespace Shard
{
    namespace Renderer
    {
        struct FieldNode
        {
            RtField        field_;
            PassHandle    pass_;
            bool operator<(const FieldNode& rhs) const {
                return this->pass_ < rhs.pass_;
            }
        };

        //think about whether new rhi proxy
        class MINIT_API RtRenderGraphExecutor
        {
        public:
            using Ptr = RtRenderGraphExecutor*;
            using SharedPtr = std::shared_ptr<RtRenderGraphExecutor>;
            using CallBack = std::function<void(RtRenderGraphExecutor& executor)>;
            using TextureHandle = TextureRepo<>::Handle;
            using BufferHandle = BufferRepo<>::Handle;
            struct _CommandContext
            {
                RHI::RHICommandContext* rhi_{ nullptr };
                RHI::RHICommandContext* async_rhi_{ nullptr };
            };
            using RHIUnionContext = _CommandContext;
        public:
            static SharedPtr Create(Utils::Allocator* alloc);
            void Execute(RHIUnionContext& context);
            void InsertPass(PassHandle handle, RtRendererPass& pass);
            //regist resource collection and barrier batch as callback
            void InsertCallBack(PassHandle time, CallBack&& call, bool is_post = false);
            const RtRendererPass& GetRenderPass(PassHandle handle) const;
            RtRenderTexture::Ptr GetRenderTexture(TextureHandle handle);
            RtRenderBuffer::Ptr GetRenderBuffer(BufferHandle handle);
            TextureHandle GetOrCreateTexture(const RtField& field);
            BufferHandle GetOrCreateBuffer(const RtField& field);
            template<class ShaderParameter, typename... Args>
            typename ShaderParameter::Ptr CreateShaderParameters(Args&&... args) {
                return AllocNoDestruct<ShaderParameter>(args);
            }
            //binding external resource to executor
            RtRenderGraphExecutor& RegistExternalResource(const RtField& field, RtRenderResource::Ptr resource);
            //assign ready flag for executor,u can do render now
            void Ready() { is_baked_ = true; }
            //whether executor ready for draw
            bool IsReady()const { return is_baked_; }
        private:
            /*todo: collection information of all memory, and do allocation*/
            void AliasResourceMemoryAlloc();
            void ExecutePass(RHI::RHICommandContext* context, RtRendererPass& pass);
            //queue extracted output resource 
            RtRenderGraphExecutor& QueueExtractedTexture(RtField& field, RtRenderTexture::Ptr& texture);
            RtRenderGraphExecutor& QueueExtractedBuffer(RtField& field, RtRenderBuffer::Ptr& buffer);
            template<class T, typename... Args>
            T* AllocNoDestruct(Args&&... args) {
                auto result = alloc_->AllocNoDestruct(args);
                return result;
            }
            RtRenderGraphExecutor(Utils::Allocator* alloc);
            DISALLOW_COPY_AND_ASSIGN(RtRenderGraphExecutor);
        private:
            friend class RtRenderGraphBuilder;
            friend class RtFieldResourcePlanner;
            using PassData = std::pair<PassHandle, RtRendererPass>;
            bool    is_baked_{ false };
            Vector<PassData>    passes_;
            Utils::Allocator*    allocator_{nullptr};
            Map<PassHandle, SmallVector<CallBack>>    pre_watch_dogs_;
            Map<PassHandle, SmallVector<CallBack>>    post_watch_dogs_;
            TextureRepo<>    texture_repo_;
            BufferRepo<>    buffer_repo_;
            Map<String, TextureHandle>    texture_map_;
            Map<String, BufferHandle>    buffer_map_;
            
        };

        class TextureSubFieldState;
        //here my idea: combin all shared edge resource together, and do resource plan
        class RtFieldResourcePlanner
        {
        public:
            RtFieldResourcePlanner() = default;
            void DoResourcePlan(RtRenderGraphExecutor& executor);
            void AppendProducer(RtField& field, PassHandle pass);
            void ApeendConsumer(RtField& field, PassHandle pass);
        private:
            void DoTexturePlan(RtRenderGraphExecutor& executor);
            void DoBufferPlan(RtRenderGraphExecutor& executor);
            void AddTransition(RtRenderGraphExecutor& executor, TextureSubFieldState& state_before, TextureSubFieldState state_after);
            void AddAlias(RtRenderGraphExecutor& executor);
        private:
            SmallVector<FieldNode, 2>    producers_;
            SmallVector<FieldNode, 2>    consumers_;

            struct AliasMemRegion
            {
                size_type   size_{ 0u };
                struct {
                    uint32_t    first_;
                    uint32_t    end_;
                }life_time_{ 0u };
            };

            struct AliasMemBucket
            {
                uint32_t    flags_{ 0u };
                //at least one region for initializing
                SmallVector<AliasMemRegion, 1u> regions_;
            };

            SmallVector<AliasMemBucket> alias_mem_bucket_;
        };
    }
}
