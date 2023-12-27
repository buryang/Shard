#include "RtRenderPass.h"

namespace Shard
{
     namespace Renderer
     {
          RtRendererPass::RtRendererPass(const String& name, EPipeLine pipeline, EFlags flags):name_(name), pipeline_(pipeline)
          {
          }

          void RtRendererPass::PreExecute()
          {
          }

          RtRendererPass& RtRendererPass::SetScheduleContext(ScheduleContext&& context)
          {
               schedule_context_ = context;
          }

          void RtRendererPass::GetInputFields(Vector<RtField>& input_fields)
          {
               auto& fields = GetSchduleContext().GetFields();
               input_fields.reserve(fields.size() / 2);
               for (auto [_, f] : fields) {
                    if (f.IsInput()) {
                         input_fields.push_back(f);
                    }
               }
          }

          void RtRendererPass::GetOutputFields(Vector<RtField>& output_fields)
          {
               auto& fields = GetSchduleContext().GetFields();
               output_fields.reserve(fields.size() / 2);
               for (auto [_, f] : fields) {
                    if (f.IsOutput()) {
                         output_fields.push_back(f);
                    }
               }
          }

          RtBarrierBatch::Ptr RtRendererPass::GetOrCreatePrologureBarrier(Utils::Allocator* alloc)
          {
               if (nullptr == barriers_prologue_) {
                    barriers_prologue_ = reinterpret_cast<RtBarrierBatch*>(alloc->Alloc(sizeof(RtBarrierBatch)));
               }
               return barriers_prologue_;
          }

          RtBarrierBatch::Ptr RtRendererPass::GetOrCreateEpilogureBarrier(Utils::Allocator* alloc)
          {
               if (nullptr == barriers_epilogure_) {
                    barriers_epilogure_ = reinterpret_cast<RtBarrierBatch*>(alloc->Alloc(sizeof(RtBarrierBatch)));
               }
               return RtBarrierBatch::Ptr();
          }

          RtRendererPass::RtScheduleContext& RtRendererPass::RtScheduleContext::AddField(RtField&& field)
          {
               auto field_name = field.GetName();
               if (fields_.find(field_name) == fields_.end()) {
                    fields_.insert(eastl::make_pair(field_name, field));
               }
               return *this;
          }

     }
}