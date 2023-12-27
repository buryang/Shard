#pragma once
#include "Utils/Handle.h"
#include "Utils/SimpleEntitySystem.h"
#include "Renderer/RtRenderBarrier.h"
#include "Renderer/RtRenderGraph.h"
#include "Renderer/RtRenderResources.h"

namespace Shard
{
        namespace Utils {
                class Allocator;
        }
        namespace Renderer
        {
                enum class EPipeLine : uint8_t
                {
                        eNone = 0x0,
                        eGraphics = 0x1,
                        eAsyncCompute = 0x2,
                        eNum = 0x3,
                };

                //frame graph render pass
                class RtRendererPass
                {
                public:
                        using Ptr = RtRendererPass*;
                        enum class EFlags : uint8_t
                        {
                                eNone        = 0x0,
                                eGFX        = 0x1,
                                eCompute        = 0x2,
                                eAsync                = 0x4,
                                eCopy                = 0x8,
                                eNerverCull        = 0x10,
                        };

                        //how to bind resource and field FIXME
                        class RtScheduleContext
                        {
                        public:
                                RtScheduleContext& AddField(RtField&& field);
                                bool IsEmpty()const { return fields_.size()==0; }
                                RtField& operator[](const String& name) {
                                        if (auto iter = fields_.find(name); iter != fields_.end()) {
                                                return iter->second;
                                        }
                                        //todo error
                                }
                                const RtField& operator[](const String& name) const {
                                        if (auto iter = fields_.find(name); iter != fields_.end()) {
                                                return iter->second;
                                        }
                                        //todo error
                                }
                                Map<String, RtField>& operator()(void) {
                                        return fields_;
                                }
                                
                                auto& GetFields() {
                                        return fields_;
                                }
                                template <typename Function>
                                void Enumerate(Function func) {
                                        for (auto& [_, p] : fields_) {
                                                func(p);
                                        }
                                }
                        private:
                                Map<String, RtField>        fields_;
                        };
                        using ScheduleContext = RtScheduleContext;

                        explicit RtRendererPass(const String& name, EPipeLine pipeline, EFlags flags);
                        virtual ~RtRendererPass();
                        virtual void Execute(RHI::RHICommandContext* context) = 0;
                        /*setup some essential prepare work*/
                        void PreExecute();
                        RtRendererPass& SetScheduleContext(ScheduleContext&& context);
                        FORCE_INLINE ScheduleContext& GetSchduleContext() { return schedule_context_; }
                        FORCE_INLINE RtField& GetField(const String& field_name) { return schedule_context_[field_name]; }
                        FORCE_INLINE bool IsOutput()const { return is_output_; }
                        FORCE_INLINE bool IsAysnc()const { return is_async_; }
                        FORCE_INLINE bool IsFeildsEmpty()const { return schedule_context_.IsEmpty(); }
                        FORCE_INLINE bool IsCullAble()const { return is_culling_able_; }
                        FORCE_INLINE const String GetName()const { return name_; }
                        FORCE_INLINE EPipeLine GetPipeLine()const { return pipeline_; }
                        static SmallVector<EPipeLine, 2, false>& GetSupportPipeLines() {
                                static SmallVector<EPipeLine, 2, false> pipe_lines{ EPipeLine::eGraphics, EPipeLine::eAsyncCompute };
                                return pipe_lines;
                        }
                        void GetInputFields(Vector<RtField>& input_fields);
                        void GetOutputFields(Vector<RtField>& output_fields);
                        RtBarrierBatch::Ptr GetOrCreatePrologureBarrier(Utils::Allocator* alloc);
                        RtBarrierBatch::Ptr GetOrCreateEpilogureBarrier(Utils::Allocator* alloc);
                        FORCE_INLINE void IncrRef() { ++ref_count_; }
                        FORCE_INLINE void DecrRef() { --ref_count_; }
                private:
                        const String        name_;
                        const EPipeLine        pipeline_;
                        uint32_t        ref_count_{ 0 };
                        EFlags        flags_{ EFlags::eNone };
                        Set<PassHandle>        producers_;
                        Set<PassHandle>        consumers_;
                        ScheduleContext        schedule_context_;

                        //render barrier
                        RtBarrierBatch::Ptr        barriers_prologue_{ nullptr };
                        RtBarrierBatch::Ptr        barriers_epilogure_{ nullptr };
                        union
                        {
                                struct
                                {
                                        //whether in a async task queue
                                        uint8_t        is_async_ : 1;
                                        //generate/wait sync fence for follow pass
                                        //whether gfx fork/join        
                                        uint8_t        is_gfx_fork_ : 1;
                                        uint8_t        is_gfx_join_ : 1;
                                        //whether async compute fork/join
                                        uint8_t        is_compute_begin_ : 1;
                                        uint8_t        is_compute_end_ : 1;
                                        //culling able
                                        uint8_t        is_culling_able_ : 1;
                                        uint8_t        is_output_ : 1;
                                };
                                uint8_t        pack_bits_{ 0 };
                        };
                        
                };

                //lambda render pass
                template <typename LAMBDA>
                requires std::is_invocable_v<LAMBDA, RHI::RHICommandContext::Ptr>
                class RtLambdaRendererPass : public RtRendererPass
                {
                public:
                        RtLambdaRendererPass(const String& name, EPipeLine pipe, EFlags flags, LAMBDA&& lamda) : RtRendererPass(name, pipe, flags), draw_bat_(lambda_) {}
                        void Execute(RHI::RHICommandContext::Ptr context) override {
                                draw_bat_(context);
                        }
                        virtual ~RtLambdaRendererPass() {}
                private:
                        LAMBDA draw_bat_;
                };

                using PassHandle = Utils::Handle<RtRendererPass>;
        }
}