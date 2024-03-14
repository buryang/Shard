#include "eastl/sort.h"
#include "Core/EngineGlobalParams.h"
#include "Render/RenderGraphExe.h"
#include "Render/RenderGraphPerfDebug.h"
#include "RHI/RHI.h"

namespace Shard
{
    namespace Render
    {

        enum EResourceAliasStrategy
        {
            eAliasNone, 
            eAliasResource, //pooled resource for aliasing
            eAliasMemory, //resident memory for aliasing
        };

        REGIST_PARAM_TYPE(UINT, RESOURCE_ALIAS_STRATEGY, EResourceAliasStrategy::eAliasNone);

        /*gfx queue and async compute queue*/
        static constexpr uint32_t MAX_QUEUE_COUNT = 2;
        struct TextureSubFieldState
        {
            EAccessFlags    state_;
            RenderPass::Handle  first_pass_[MAX_QUEUE_COUNT];
            RenderPass::Handle  last_pass_[MAX_QUEUE_COUNT];
            TextureSubFieldState() noexcept{
                for (auto n = 0; n < MAX_QUEUE_COUNT; ++n) {
                    first_pass_[n] = last_pass_[n] = (RenderPass::PassHandle)-1;
                }
            }
            TextureSubFieldState(const EAccessFlags& state) noexcept:state_(state) {
                for (auto n = 0; n < MAX_QUEUE_COUNT; ++n) {
                    first_pass_[n] = last_pass_[n] = (RenderPass::PassHandle)-1;
                }
            }
            bool IsValid() const {
                //first pass and last pass at least one not equal -1
                uint32_t null_count = 0;
                for (const auto& handle : first_pass_) {
                    null_count += handle == -1;
                }
                if (null_count == MAX_QUEUE_COUNT) {
                    return false;
                }
                null_count = 0;
                for (const auto& handle : last_pass_) {
                    null_count += handle == -1;
                }
                return (null_count != MAX_QUEUE_COUNT);
            }
            void Reset() {
                *this = {};
            }
        };

        template<typename State>
        static void InitWholeTextureSubStates(const State& init_state, SmallVector<TextureSubFieldState>& states) {
            states.resize(1);
            states[0] = init_state;
        }

        template<typename State>
        static void InitAllTextureSubStates(const TextureDim& layout, const State& init_state, SmallVector<TextureSubFieldState>& states) {
            const auto states_count = TextureSubRange(layout).GetSubRangeIndexCount();
            states.resize(states_count);
            for (auto n = 0; n < states_count; ++n) {
                states[n] = init_state;
            }
        }


        RenderGraphExecutor::SharedPtr RenderGraphExecutor::Create(Utils::Allocator* alloc)
        {
            SharedPtr executor(new RenderGraphExecutor(alloc));
            return executor;
        }

        void RenderGraphExecutor::Bake(const RenderGraph& render_graph)
        {
            //copy ordered passes and resource map
            std::copy(render_graph.ordered_passes_.begin(), render_graph.ordered_passes_.end(), ordered_passes_.end());
            std::copy()

            AnalyseAliasResourceMemoryAlloc();
            AllocateRHIResources();
        }

        void RenderGraphExecutor::Execute(RHIUnionContext& context)
        {
            for (auto& [handle, pass] : passes_)
            {
                auto curr_context = pass.IsAysnc() ? context.async_rhi_ : context.rhi_;
                for (auto& call_back : pre_watch_dogs_[pass]) {
                    call_back(*this, pass);
                }
                ExecutePass(curr_context, pass);
                for (auto& call_back : post_watch_dogs_[pass]) {
                    call_back(*this, pass);
                }
            }

        }

        void RenderGraphExecutor::InsertPass(PassHandle pass_handle, RenderPass& pass)
        {
            PassData pass_data{ pass_handle, pass };
            passes_.emplace_back(pass_data);
            pass.PreExecute();
        }

        const RenderPass& RenderGraphExecutor::GetRenderPass(PassHandle handle) const
        {
            auto pass_iter = eastl::find_if(passes_.begin(), passes_.end(), [&](auto data) {
                return data.first == handle; });
            assert(pass_iter != passes_.end());
            return pass_iter->second;
        }

        RenderTexture::Ptr RenderGraphExecutor::GetRenderTexture(TextureHandle handle)
        {
            return texture_repo_.Get(handle);
        }

        RenderBuffer::Ptr RenderGraphExecutor::GetRenderBuffer(BufferHandle handle)
        {
            return buffer_repo_.Get(handle);
        }

        RenderGraphExecutor::TextureHandle RenderGraphExecutor::GetOrCreateTexture(const Field& field)
        {
            const String& key = field.GetParentName();
            if (texture_map_.find(key) == texture_map_.end()) {
                //transform field to desc
                TextureHandle texture_handle = texture_repo_.Alloc(field);
                texture_map_.insert(eastl::make_pair(key, texture_handle));
            }
            return texture_map_[key];
        }

        RenderGraphExecutor::BufferHandle RenderGraphExecutor::GetOrCreateBuffer(const Field& field)
        {
            const String& key = field.GetParentName();
            if (buffer_map_.find(key) == buffer_map_.end()) {
                //transform field to desc
                BufferHandle buffer_handle = buffer_repo_.Alloc(field);
                buffer_map_.insert(eastl::make_pair(key, buffer_handle));
            }
            return buffer_map_[key];
        }

        RenderGraphExecutor& RenderGraphExecutor::RegistExternalResource(const Field& field, RenderResource::Ptr external_resource)
        {
            //alloc a same configure resource as resource
            const auto& field_name = field.GetName();
            if (field.GetType() == Field::EType::eBuffer) {
                auto* external_buffer = dynamic_cast<RenderBuffer*>(external_resource);
                auto buffer_handle = buffer_repo_.Alloc(*external_buffer);//shallow copy, use rhi as external resource
                buffer_map_.insert(eastl::make_pair(field_name, buffer_handle));
            }
            else
            {
                auto* external_texture = dynamic_cast<RenderTexture*>(external_resource);
                auto texture_handle = texture_repo_.Alloc(*external_texture);
                texture_map_.insert(eastl::make_pair(field_name, texture_handle));
            }
            return *this;
        }

        void RenderGraphExecutor::InsertCallBack(PassHandle time, CallBack&& call, bool is_post)
        {
            auto& watch_dogs = is_post ? post_watch_dogs_ : pre_watch_dogs_;
            if (watch_dogs.find(time) == watch_dogs.end()) {
                watch_dogs[time] = { call };
            }
            else
            {
                watch_dogs[time].emplace_back(call);
            }
        }

        void RenderGraphExecutor::ExecutePass(RHI::RHICommandContext* context, RenderPass& pass)
        {
            RENDER_EVENT("render_pass:%s", pass.GetName());
            pass.Execute(context);
        }

        RenderGraphExecutor& RenderGraphExecutor::QueueExtractedTexture(Field& field, RenderTexture::Ptr& extracted_texture)
        {
            assert(field.IsOutput() && "extracted field should be output");
            auto texture_handle = GetOrCreateTexture(field);
            auto& texture = 0;
            extracted_texture->SetRHI();
            return *this;
        }

        bool RenderGraphExecutor::IsReady() const
        {
            return is_baked_;
        }

        void FieldResourcePlanner::DoResourcePlan(RenderGraphExecutor& executor)
        {
            if (producers_[0].field_.GetType() == Field::EType::eBuffer) {
                DoBufferPlan(executor);
            }
            else {
                DoTexturePlan(executor);
            }
        }

        void FieldResourcePlanner::DoTexturePlan(RenderGraphExecutor& executor)
        {
            //pingpong buffer for merge state
            SmallVector<TextureSubFieldState> merged_states_buffer[2];
            auto& merged_states_ping = merged_states_buffer[0];
            auto& merged_states_pong = merged_states_buffer[1];
            //first sort producers and consumter
            SmallVector<FieldNode, 4> combined_nodes;

            combined_nodes.insert(combined_nodes.end(), producers_.begin(), producers_.end());
            combined_nodes.insert(combined_nodes.end(), consumers_.begin(), consumers_.end());
            eastl::sort(combined_nodes.begin(), combined_nodes.end());

            auto is_pipeline_equal = [&](PassHandle lhs, PassHandle rhs) {
                auto lhs_pipe = executor.GetRenderPass(lhs).GetPipeLine();
                auto rhs_pipe = executor.GetRenderPass(rhs).GetPipeLine();
                return lhs_pipe == rhs_pipe;
            };

            auto init_field_state = [&](TextureSubFieldState& merge_state, Field::EAccessFlags state, PassHandle pass_handle) {
                merge_state.state_ = state;
                const auto pipeline = static_cast<uint32_t>(executor.GetRenderPass(pass_handle).GetPipeLine());
                merge_state.first_pass_[pipeline] = merge_state.last_pass_[pipeline] = pass_handle;
            };
            auto merge_field_state = [&](TextureSubFieldState& merge_state, Field::EAccessFlags state, PassHandle pass_handle) {
                merge_state.state_ = Utils::LogicOrFlags(merge_state.state_, state);
                const auto pipeline = static_cast<uint32_t>(executor.GetRenderPass(pass_handle).GetPipeLine());
                if (merge_state.first_pass_[pipeline] == -1) {
                    merge_state.first_pass_[pipeline] = pass_handle;
                }
                merge_state.last_pass_[pipeline] = pass_handle;
            };

            auto iter = combined_nodes.begin();
            auto prologue_pass = iter->pass_;
            Field prologue_field{ (*iter++).field_ };
            const auto& field_layout = prologue_field.GetLayout();
            if (prologue_field.IsWholeResource()) {
                InitWholeTextureSubStates(prologue_field.GetSubField().access_, merged_states_ping);
            }
            else {
                //todo fixme pass handle assign
                InitAllTextureSubStates(prologue_field.GetLayout(), prologue_field.GetSubField().access_, merged_states_ping);
            }

            auto texture_handle = executor.GetOrCreateTexture(prologue_field);
            //insert prologue resource release handle
            executor.InsertCallBack(prologue_pass, [&](RenderGraphExecutor& executor) {
                auto& pass = executor.GetRenderPass(prologue_pass);
                auto texture = executor.GetRenderTexture(texture_handle);
                //begin texture RHI
                texture->SetRHI();
                }, false);

            for (; iter != combined_nodes.end(); ++iter) {
                auto curr_pass_handle = iter->pass_;
                auto curr_pass = executor.GetRenderPass(curr_pass_handle);
                //range change firstly if needed
                if (!iter->field_.GetSubRange().IsWholeRange(field_layout)) {
                    if (merged_states_ping.size() == 1) {
                        InitAllTextureSubStates(prologue_field.GetLayout(), merged_states_ping[0], merged_states_ping);
                    }
                    //1->N & N->N
                    iter->field_.GetSubRange().For([&](const auto& range_index) {
                        auto merge_index = TextureSubRange(field_layout).GetSubRangeIndexLocation(range_index);
                        auto& merge_state = merged_states_ping[merge_index];
                        const auto& state = iter->field_.GetSubField().access_;
                        if (Field::IsSubFieldMergeAllowed({ merge_state.state_ }, { state })) {
                            merge_field_state(merge_state, state, curr_pass_handle);
                        }
                        if (Field::IsSubFieldTransitionNeeded(merge_state.state_, state)) {
                            //check wheter prev transion need finish
                            auto& prev_merge_state = merged_states_pong[merge_index];
                            if (prev_merge_state.IsValid()) {
                                AddTransition(executor, prev_merge_state, merge_state);
                                prev_merge_state.Reset();
                            }
                            std::swap(prev_merge_state, merge_state);
                            init_field_state(merge_state, state, curr_pass_handle);
                        }
                    });
                }
                else
                {
                    //1->1
                    if (merged_states_ping.size() == 1) {
                        auto& merge_state = merged_states_ping[0];
                        const auto& state = iter->field_.GetSubField().access_;
                        if (Field::IsSubFieldMergeAllowed({ merge_state.state_ }, { state })) {
                            merge_field_state(merge_state, state, curr_pass_handle);
                        }
                        if (Field::IsSubFieldTransitionNeeded({ merge_state.state_ }, { state })) {
                            auto& prev_merge_state = merged_states_pong[0];
                            if(prev_merge_state.IsValid()){
                                AddTransition(executor, prev_merge_state, merge_state);
                                prev_merge_state.Reset();
                            }
                            std::swap(prev_merge_state, merge_state);
                            init_field_state(merge_state, state, curr_pass_handle);
                            
                        }
                    }
                    else //N->1
                    {
                        TextureSubRange(field_layout).For([&](const auto& range_index)) {
                            auto merge_index = TextureSubRange(field_layout);
                            auto& merge_state = merged_states[merge_index].state_;
                            const auto& state = iter->field_.GetSubField().access_;
                            if (Field::IsSubFieldMergeAllowed()) {

                            }
                            if (Field::IsSubFieldTransitionNeeded()) {

                            }
                        });
                    }
                }
                //flush merge state pong and insert epilogue resource release handle
                {
                    for (auto n = 0; n < merged_states_pong.size(); ++n) {
                        auto& prev_merge_state = merged_states_pong[n];
                        auto& merge_state = merged_states_ping[n];
                        if (prev_merge_state.IsValid()) {
                            AddTransition(executor, prev_merge_state, merge_state);
                        }
                    }
                    auto epilogure_pass = combined_nodes.back().pass_;
                    executor.InsertCallBack(epilogure_pass, [&](RenderGraphExecutor& executor) {
                        auto& pass = executor.GetRenderPass(epilogure_pass);
                        auto texture = executor.GetRenderTexture(texture_handle);
                        //end texture RHI
                        texture->EndRHI();
                    }, true);
                }

            }
        }
        void FieldResourcePlanner::DoBufferPlan(RenderGraphExecutor& executor)
        {

        }

        void FieldResourcePlanner::AddTransition(RenderGraphExecutor& executor, TextureSubFieldState& state_before, TextureSubFieldState state_after)
        {
            auto is_multique_pass = [](auto passes) {
                uint32_t valid_count = 0;
                for (auto n = 0; n < MAX_QUEUE_COUNT; ++n) {
                    valid_count += passes[n] != -1;
                }
                return valid_count > 1;
            };

            if (!is_multique_pass(state_before.first_pass_)) {
                for (auto n = 0; n < MAX_QUEUE_COUNT; ++n) {
                    //find the only valid queue
                    if (state_before.first_pass_[n] == -1) {
                        continue;
                    }
                    const auto last_pass_before = state_before.last_pass_[n];
                    auto& barrier_batch_begin = executor.GetRenderPass(last_pass_before).;
                    //1->1
                    if (!is_multique_pass(state_after.first_pass_)) {
                        auto first_pass_after = *eastl::find_if(state_after.first_pass_, state_after.first_pass_ + MAX_QUEUE_COUNT, [](auto val) { return val != -1 });
                        auto& barrier_batch_end = executor.GetRenderPass(first_pass_after).;
                    }
                    else //1->N
                    {
                        //copy from ue, end as beginner's step
                        auto& barrier_batch_end = executor.GetRenderPass(last_pass_before).;
                    }
                }
            }
            else
            {
                //N->1
                if (!is_multique_pass(state_after.first_pass_)) {
                    for (auto n = 0; n < MAX_QUEUE_COUNT; ++n) {
                        const auto last_pass_before = state_before.last_pass_[n];
                        if (last_pass_before != -1) {

                        }
                    }
                }
                else //N->N
                {
                    for (auto n = 0; n < MAX_QUEUE_COUNT; ++n) {

                    }
                }
            }
        }
        void FieldResourcePlanner::AppendProducer(Field & field, PassHandle pass)
        {
            FieldNode producer{ field, pass };
            producers_.emplace_back(producer);
        }
        void FieldResourcePlanner::ApeendConsumer(Field & field, PassHandle pass)
        {
            FieldNode consumer{ field, pass };
            consumers_.emplace_back(consumer);
        }
        RenderGraphExecutor::RenderGraphExecutor(Utils::Allocator* alloc):allocator_(alloc), texture_repo_(*alloc), buffer_repo_(*alloc) {
        }

    }
}