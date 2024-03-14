#pragma once
#include "Utils/Handle.h"
#include "Utils/CommonUtils.h"
#include "Utils/SimpleEntitySystem.h"
#include "Render/RenderBarrier.h"
#include "Render/RenderGraph.h"
#include "Render/RenderResources.h"

namespace Shard
{
    namespace Render
    {
        class RenderGraphExecutor;
        //frame graph render pass
        class RenderPass
        {
        public:
            using Ptr = RenderPass*;
            enum class EFlags : uint8_t
            {
                eNone   = 0x0,
                eGFX    = 0x1,
                eCompute    = 0x2,
                eAsync  = 0x4,
                eCopy   = 0x8,
                eNerverCull = 0x10,
            };

            //how to bind resource and field FIXME
            class ScheduleContext
            {
            public:
                ScheduleContext() = default;
                ScheduleContext(ScheduleContext&& other)noexcept;
                ScheduleContext& operator=(ScheduleContext&& other)noexcept;
                ScheduleContext& AddField(Field&& field);
                bool IsEmpty()const { return fields_.empty(); }
                void EnumerateInputFields(SmallVector<Field>& input_fields);
                void EnumerateOutputFields(SmallVector<Field>& output_fields);
                Field* operator[](const String& name) {
                    if (auto iter = eastl::find_if(fields_.begin(), fields_.end(), [](auto iter) { return iter->GetName() == name; }); iter != fields_.end()) {
                        return &(*iter);
                    }
                    return nullptr;
                }
                const Field* operator[](const String& name) const {
                    if (auto iter = eastl::find_if(fields_.cbegin(), fields_.cend(), [](const auto iter) { return iter->GetName() == name; }); iter != fields_.cend()) {
                        return &(*iter);
                    }
                    return nullptr;
                } 
                ScheduleContext& operator+=(ScheduleContext&& other) {
                    fields_.merge(other.fields_); //it'll keep all different element in *this
                }
                const auto& GetFields()const {
                    return fields_;
                }
                auto& GetFields() {
                    return fields_;
                }
                template <typename Function>
                void Enumerate(Function func) {
                    for (auto& fd: fields_) {
                        func(fd);
                    }
                }
            private:
                Set<Field>   fields_;
            };

            //pass and edge resource manager
            using PassRepo = Utils::ResourceManager<RenderPass, Utils::ScalablePoolAllocator>;
            using Handle = PassRepo::Handle;

            /*global pass create instance*/
            static PassRepo& PassRepoInstance();
        
            explicit RenderPass(const String& name, EPipeline pipeline, EFlags flags);
            virtual ~RenderPass();
            /*setup some essential prepare work*/
            virtual void PreExecute() {};
            virtual void Execute(RenderGraphExecutor* executor, RHI::RHICommandContext* context) = 0;
            virtual void PostExecute() {}
            virtual void Serialize(Utils::IOArchive& ar);
            FORCE_INLINE ScheduleContext& GetScheduleContext() { return schedule_context_; }
            FORCE_INLINE Field& GetField(const String& field_name) { return schedule_context_[field_name]; }
            FORCE_INLINE bool IsOutput()const { return is_output_; }
            FORCE_INLINE bool IsAysnc()const { return is_async_; }
            FORCE_INLINE bool IsCullAble()const { return is_culling_able_; }
            FORCE_INLINE const String& GetName()const { return name_; }
            FORCE_INLINE EPipeline GetPipeline()const { return pipeline_; }
            FORCE_INLINE void SetDependencyLevel(uint32_t level) { dependency_level_ = level; }
            FORCE_INLINE uint32_t GetDependencyLevel()const { return dependency_level_; }
            virtual EPipeline GetSupportPipelines()const {
                return EPipeline::eAll;
            }
            FORCE_INLINE BarrierBatch& GetPrologureBarrier() { return barriers_prologue_; }
            FORCE_INLINE BarrierBatch& GetEpilogureBarrier() { return barriers_epilogure_; }
            FORCE_INLINE void IncrRef() { ++ref_count_; }
            FORCE_INLINE void DecrRef() { --ref_count_; }

        private:
            const String    name_;
            const EPipeline pipeline_;
            uint32_t    ref_count_{ 0u };
            uint32_t    dependency_level_{ 0u };
            EFlags  flags_{ EFlags::eNone };
            Set<RenderPass::Handle> producers_;
            Set<RenderPass::Handle> consumers_;
            ScheduleContext schedule_context_;

            //render barrier
            BarrierBatch  barriers_prologue_;
            BarrierBatch  barriers_epilogure_;

            union
            {
                struct
                {
                    //whether in a async task queue
                    uint8_t    is_async_ : 1;
                    //generate/wait sync fence for follow pass
                    //whether gfx fork/join    
                    uint8_t    is_gfx_fork_ : 1;
                    uint8_t    is_gfx_join_ : 1;
                    //whether async compute fork/join
                    uint8_t    is_compute_begin_ : 1;
                    uint8_t    is_compute_end_ : 1;
                    //culling able
                    uint8_t    is_culling_able_ : 1;
                    uint8_t    is_output_ : 1;
                };
                uint8_t    pack_bits_{ 0u };
            };
        };

        //lambda render pass
        template <typename LAMBDA>
        requires std::is_invocable_v<LAMBDA, RHI::RHICommandContext::Ptr>
        class LambdaRenderPass : public RenderPass
        {
        public:
            LambdaRenderPass(const String& name, EPipeline pipe, EFlags flags, LAMBDA&& lamda) : RenderPass(name, pipe, flags), draw_bat_(std::move(lambda_)) {}
            void Execute(RenderGraphExecutor* executor, RHI::RHICommandContext::Ptr context) override {
                draw_bat_(executor, context);
            }
            virtual ~LambdaRenderPass() {}
        private:
            LAMBDA draw_bat_;
        };

        class NullRenderPass final : public RenderPass
        {
        public:
            NullRenderPass(const String& name, EPipeline pipe, EFlags flags) :RenderPass(name, pip, flags) {}
            void Execute(RenderGraphExecutor* executor, RHI::RHICommandContext::Ptr context) override {
            }
        };

        class PackedRenderPass final : public RenderPass
        {
        public:
            PackedRenderPass(const String& name, Span<RenderPass::Handle>& passes);
            void Execute(RenderGraphExecutor* executor, RHI::RHICommandContext::Ptr context) override;
            ~PackedRenderPass();
        private:
            SmallVector<RenderPass::Handle> sub_passes_;
        };

    }
}