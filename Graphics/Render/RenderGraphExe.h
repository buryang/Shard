#pragma once

#include "Utils/Handle.h"
#include "Utils/Memory.h"
#include "Utils/CommonUtils.h"
#include "Core/EngineGlobalParams.h"
#include "RHI/RHIGlobalEntity.h"
#include "RenderResources.h"
#include "RenderPass.h"


REGIST_PARAM_TYPE(UINT, RENDER_DEFRAG_PASS, 0);

namespace Shard
{
    namespace Render
    {
        struct FieldNode
        {
            Field   field_;
            PassHandle  pass_;
            bool operator<(const FieldNode& rhs) const {
                return this->pass_ < rhs.pass_;
            }
        };

        //think about whether new rhi proxy
        class MINIT_API RenderGraphExecutor
        {
        public:
            using Ptr = RenderGraphExecutor*;
            using SharedPtr = std::shared_ptr<RenderGraphExecutor>;
            using CallBack = std::function<void(RenderGraphExecutor& executor)>;
            using TextureHandle = TextureRepo::Handle;
            using BufferHandle = BufferRepo::Handle;
            typedef struct _CommandContext
            {
                RHI::RHICommandContext* rhi_{ nullptr };
                RHI::RHICommandContext* async_rhi_{ nullptr };
            }RHIUnionContext;
        public:
            void Bake(const RenderGraph& render_graph);
            //queue extracted output resource 
            bool QueueExtractedTexture(Field& field, RenderTexture::Handle& texture);
            bool QueueExtractedBuffer(Field& field, RenderBuffer::Handle& buffer);
            bool RegistExternalBuffer(const Field::Name& external_field, RenderBuffer::Handle buffer);
            bool RegistExternalTexture(const Field::Name& external_field, RenderTexture::Handle buffer);

            void Execute(RHIUnionContext& context);
            TextureHandle GetOrCreateTexture(const Field& field);
            BufferHandle GetOrCreateBuffer(const Field& field);
            template<class ShaderParameter, typename... Args>
            typename ShaderParameter::Ptr CreateShaderParameters(Args&&... args) {
                return AllocNoDestruct<ShaderParameter>(args);
            }
            //whether executor ready for draw, check bake status and resource status
            bool IsReady()const { return is_baked_; }
        private:
            friend class RenderGraphBuilder;
            /*todo: collection information of all memory, and do allocation*/
            void AnalyseAliasResourceMemoryAlloc();
            void AllocateRHIResources();
            void ExecutePass(RHI::RHICommandContext* context, RenderPass& pass);
            template<class T, typename... Args>
            T* AllocNoDestruct(Args&&... args) {
                auto result = alloc_->AllocNoDestruct(args);
                return result;
            }
            DISALLOW_COPY_AND_ASSIGN(RenderGraphExecutor);
        private:
            friend class RenderGraphBuilder;
            friend class FieldResourcePlanner;
            Map<Field::Name, RenderTexture::Handle> texture_map_;
            Map<Field::Name, RenderBuffer::Handle>  buffer_map_;
            Map<Field::Name, RenderTexture::Handle> external_texture_map_;
            Map<Field::Name, RenderBuffer::Handle>  external_buffer_map_;
            //ordered pass as executing order
            Vector<RenderPass::Handle>  ordered_pass_;
        };

        class TextureSubFieldState;
        //here my idea: combin all shared edge resource together, and do resource plan
        class FieldResourcePlanner
        {
        public:
            FieldResourcePlanner() = default;
            void DoResourcePlan(RenderGraphExecutor& executor);
            void AppendProducer(Field& field, RenderPass::Handle pass);
            void ApeendConsumer(Field& field, RenderPass::Handle pass);
        private:
            void DoTexturePlan(RenderGraphExecutor& executor);
            void DoBufferPlan(RenderGraphExecutor& executor);
            void AddTransition(RenderGraphExecutor& executor, TextureSubFieldState& state_before, TextureSubFieldState state_after);
            void AddAlias(RenderGraphExecutor& executor);
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
