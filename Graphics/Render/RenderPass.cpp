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

    PassRepo& RenderPass::PassRepoInstance()
    {
        static PassRepo pass_repo{ g_render_allocator };
        return pass_repo;
    }

    RenderPass::RenderPass(const String& name, EPipeline pipeline, bool never_culled):name_(name), pipeline_(pipeline), is_culling_able_(!never_culled)
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
            ar >> 
        }
        else
        {

        }
    }

    void RenderPass::GenerateHALPassInitializer(HAL::HALRenderPassInitializer& initializer)
    {
        //todo from schedule to initializer
    }

    void RenderPass::PostCompile()
    {
        if (pipeline_ == EPipeline::eAsyncCompute) {
            if (std::all_of(producers_.cbegin(), producers_.cend(), [](const auto& p) {})) {
                is_compute_fork_ = 1;
            }
            if (std::all_of()) {
                is_compute_join_ = 1;
            }
        }

        if (pipeline_ == EPipeline::eGFX) {
            if (std::all_of(producers_)) {
                is_gfx_fork_ = 1;
            }
            if () {

            }
        }
    }

    void TextureResolveRenderPass::Init()
    {
        auto& schedule_context = GetScheduleContext();
        Field input, output;
        input.Name(fmt::format("{}.{}", GetName(), "MSAAInput")).Width().Height().SampleCount(1);
        output.Name(fmt::format("{}.{}", GetName(), "MSAAOutput")).Width().Height().SampleCount(1);
        schedule_context.AddField(std::move(input)).AddField(std::move(output));
    }

    void TextureResolveRenderPass::Execute(HAL::HALCommandContext* context, RenderResourceCache* resource_cache)
    {
        auto& schedule_context = GetScheduleContext();
        auto msaa_input = resource_cache->TryGetTexture(schedule_context[fmt::format("{}.{}", GetName(), "MSAAInput")]->GetHashName());
        auto msaa_output = resource_cache->TryGetTexture(schedule_context[fmt::format("{}.{}", GetName(), "MSAAOutput")]->GetHashName());
        HAL::HALMSAAResolvePacket resolve_msaa{};
        resolve_msaa.src_texture_ = msaa_input->GetHAL();
        resolve_msaa.dst_texture_ = msaa_output->GetHAL();
        context->Enqueue(&resolve_msaa);
    }

    PackedRenderPass::PackedRenderPass(const String& name, Span<RenderPass::Handle>& passes, EPipeline pipline, bool never_culled) :RenderPass(name, pipline, never_culled) {
        for (auto pass : passes) {
            sub_passes_.emplace_back(pass);
        }
    }

    void PackedRenderPass::Init() {
        //first merge all schedule context
        for (auto n = 0u; n < sub_passes_.size(); ++n) {
            //todo
        }
        sub_passes_info_.resize(sub_passes_.size());
        //second generate subpass descriptions
        for (auto n = 0u; n < sub_passes_.size(); ++n) {
            auto subpass = sub_passes_[n];
            auto& subpass_info = sub_passes_info_[n];
            
            subpass->Init();
            //merge context and barriers
            GetScheduleContext() += std::move(subpass->GetScheduleContext());
            //shold omit barriers between subpass, while generate subpass dependency
            //we can forget about all barriers which need to happen between subpasses
            //GetPrologureBarrier() += std::move(pass->GetPrologureBarrier());
            //GetEpilogueBarrier() += std::move(pass->GetEpilogueBarrier());
        }
    }
    void PackedRenderPass::Execute(HAL::HALCommandContext* context, RenderResourceCache* resource_cache)
    {
        for (auto n = 0u; n < sub_passes_.size(); ++n) {
            auto subpass = sub_passes_[n];
            subpass->Execute(context, resource_cache);
            if (n != sub_passes_.size() - 1) {
                HAL::HALNextSubPassPacket next_subpass_info;
                context->Enqueue(&next_subpass_info);
            }
        }
    }

    void PackedRenderPass::UnInit() {
        for (auto pass : sub_passes_) {
            pass->UnInit();
        }
    }

    PackedRenderPass::~PackedRenderPass()
    {
        UnInit();
        for (auto subpass : sub_passes_) {
            RenderPass::PassRepoInstance().Free(subpass);
        }
    }

    void PackedRenderPass::GenerateHALPassInitializer(HAL::HALRenderPassInitializer& initializer)
    {
        RenderPass::GenerateHALPassInitializer(initializer);

        //generate subpass info 
        
    }



}