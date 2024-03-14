#include "RenderPass.h"

namespace Shard::Render
{

    void RenderPass::ScheduleContext::EnumerateInputFields(SmallVector<Field>& input_fields)
    {
        auto& fields = GetScheduleContext().GetFields();
        input_fields.reserve(fields.size() / 2);
        for (const auto& f : fields) {
            if (f.IsInput()) {
                input_fields.push_back(f);
            }
        }
    }

    void RenderPass::ScheduleContext::EnumerateOutputFields(SmallVector<Field>& output_fields)
    {
        auto& fields = GetScheduleContext().GetFields();
        output_fields.reserve(fields.size() / 2);
        for (const auto& f : fields) {
            if (f.IsOutput()) {
                output_fields.push_back(f);
            }
        }
    }

    RenderPass::RenderPass(const String& name, EPipeline pipeline, EFlags flags):name_(name), pipeline_(pipeline). flags_(flags)
    {
    }

    RenderPass::PassRepo& RenderPass::PassRepoInstance()
    {
        static PassRepo pass_repo;
        return pass_repo;
    }

    RenderPass::~RenderPass()
    {

    }

    RenderPass::ScheduleContext::ScheduleContext(ScheduleContext&& other) noexcept
    {
        fields_ = std::move(other.fields_);
    }

    RenderPass::ScheduleContext& RenderPass::ScheduleContext::operator=(ScheduleContext&& other) noexcept
    {
        fields_ = std::move(other.fields_);
        return *this;
    }

    RenderPass::ScheduleContext& RenderPass::ScheduleContext::AddField(Field&& field)
    {
        fields_.insert(field);
        return *this;
    }

    void RenderPass::Serialize(Utils::IOArchive& ar)
    {
        if (ar.IsReading()) {

        }
        else
        {

        }
    }
    PackedRenderPass::PackedRenderPass(const String& name, Span<RenderPass::Handle>& passes) :RenderPass(name, , ) {
        for (auto pass : passes) {
            //merge context and barriers
            GetScheduleContext() += pass->GetScheduleContext();
            GetPrologureBarrier() += pass->GetPrologureBarrier();
            GetEpilogureBarrier() += pass->GetEpilogureBarrier();
            //todo merge producers and consumers
            sub_passes_.emplace_back(pass);
        }
    }

    void PackedRenderPass::Execute(RenderGraphExecutor* executor, RHI::RHICommandContext::Ptr context)
    {
        for (auto subpass : sub_passes_) {
            //begin subpass todo
            subpass->Execute(executor, context);
            //end subpass
        }
    }

    PackedRenderPass::~PackedRenderPass()
    {
        for (auto subpass : sub_passes_) {
            RenderPass::PassRepoInstance().Free(subpass);
        }
    }

}