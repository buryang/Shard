/*****************************************************************//**
 * \file   RenderGraphExe.cpp
 * \brief  realize GPU memory aliasing algorithm from Pavlo Muratov
 * url:https://levelup.gitconnected.com/gpu-memory-aliasing-45933681a15e
 * \author buryang
 * \date   March 2024
 *********************************************************************/
#include "eastl/sort.h"
#include "Core/EngineGlobalParams.h"
#include "Render/RenderGraphExe.h"
#include "Render/RenderGraphPerfDebug.h"
#include "HAL/HALGlobalEntity.h"

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

        /*when to allocator it*/
        Utils::ScalablePoolAllocator<uint8_t>* g_render_allocator = nullptr;

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

        void RenderGraphExecutor::Execute()
        {
            const auto queue_count = 1u;
            Vector<Vector<HAL::HALCommandContext*>> queue_commands{ queue_count };

            for (auto& pass : ordered_passes_)
            {
                auto* curr_context = HAL::HALGlobalEntity::Instance()->CreateCommandBuffer(); //todo 
                const auto queue_index = pass->QueueIndex();
                queue_commands[queue_index].emplace_back(curr_context);
                
                Utils::Schedule([&, this]() {

                    ExecutePassPrologue(curr_context, pass);
                    ExecutePass(curr_context, pass);
                    ExecutePassEpilogue(curr_context, pass);
                });
            }

            //submit all command for each queue
            for (auto n = 0u; n < queue_count; ++n) {
                //todo
            }
            //clear all transient resource
            RenderPass::PassRepoInstance().Clear(); //clear all pass resource
            for (auto texture : texture_repos_) {
                if (!texture->IsOutput() && !texture->IsExternal()) {
                    RenderTexture::TextureRepoInstance().Free(texture);
                }
            }
            for (auto buffer : buffer_repos_) {
                if (!buffer->IsOutput() && !buffer->IsExternal()) {
                    RenderBuffer::BufferRepoInstance().Free(buffer);
                }
            }
        }

        RenderGraphExecutor& RenderGraphExecutor::RegistExternalResource(const Field& field, RenderResource* external_resource)
        {
            //alloc a same configure resource as resource
            const auto& field_name = field.GetName();
            if (field.GetType() == Field::EType::eBuffer) {
                auto* external_buffer = dynamic_cast<RenderBuffer*>(external_resource);
                auto buffer_handle = buffer_repos_.Alloc(*external_buffer);//shallow copy, use rhi as external resource
                buffer_map_.insert(eastl::make_pair(field_name, buffer_handle));
            }
            else
            {
                auto* external_texture = dynamic_cast<RenderTexture*>(external_resource);
                auto texture_handle = texture_repos_.Alloc(*external_texture);
                texture_map_.insert(eastl::make_pair(field_name, texture_handle));
            }
            return *this;
        }



        void RenderGraphExecutor::AnalyseRenderResourceResidency()
        {
            //traverse all transient resource 
            MemoryAliasCollector collector;
            EnumerateContainer(texture_repos_, [&collector](auto texture) {
                if (texture->IsTransient()) {
                    collector.Enqueue();
                }
            });
            
            EnumerateContainer(buffer_repos_, [&collector](auto buffer) {
                if (buffer->IsTransient()) {
                    collector.Enqueue();
                }
            });
            {
                RENDER_EVENT("transient resource alias analyse");
                collector.DoAnalyse();
            }

            //allocate memory
            auto* residency_manager = HAL::HALGlobalEntity::Instance()->GetOrCreateMemoryResidencyManager();
            assert(residency_manager != nullptr);
            for (const auto& bucket_info : collector.GetBuckets()) {
                HAL::HALAllocationCreateInfo create_info{ .size_ = bucket_info.size_ };
                alias_allocations_.push_back(residency_manager->AllocMemory(create_info));
            }

            //make transient resource residency
            EnumerateContainer(texture_repos_, [&, this](auto texture) {
            });
            EnumerateContainer(buffer_repos_, [&, this](auto buffer) {
            });
        }

        void RenderGraphExecutor::AnalyseRenderResourceSync()
        {
            for (auto pass : ordered_passes_) {
                {
                    SmallVector<Field> outputs;
                    pass->GetScheduleContext().EnumerateOutputFields(outputs);
                    EnumerateContainer(outputs, [&, this](auto& field) {
                        pass->GetPrologureBarrier().AddBarrier();
                    });
                }
                {
                    SmallVector<Field> inputs;
                    pass->GetScheduleContext().EnumerateInputFields(inputs);
                    EnumerateContainer(inputs, [&, this](auto& field) {
                    });
                }
            }
        }

        void RenderGraphExecutor::ExecutePassPrologue(HAL::HALCommandContext* context, RenderPass::Handle pass)
        {
            //first do sync & transition
            auto prologue_barrier = pass->GetPrologureBarrier();
            if (!prologue_barrier.IsEmpty()) {
                if(prologue_barrier.type_ == EBarrierBatchType::eSplitBegin)
                {
                    HAL::HALSplitBarrierBeginPacket barrier_info;
                    context->Enqueue(&barrier_info);

                }
                else if (prologue_barrier.type_ == EBarrierBatchType::eNormal)
                {
                    HAL::HALBarrierPacket barrier_info;
                    context->Enqueue(&barrier_info);
                }
                else
                {
                    LOG(ERROR) << fmt::format("barrier type{} is not support here", BarrierBatchTypeToChar(prologue_barrier.type_);
                }
            }
            HAL::HALBeginRenderPassPacket begin_pass_info;
            pass->GetScheduleContext().Enumerate([&begin_pass_info](auto& field) {
                if (Utils::HasAnyFlags(field.GetSubFieldState().access_, EAccessFlags::eDSV)) {
                    begin_pass_info.dsv_attach_exist_ = true;
                    begin_pass_info.depth_stencil_.texture_ = resource_cache_->GetOrCreateTexture(field.HashName()); //todo
                }
                else if (Utils::HasAnyFlags(field.)) {
                    auto& color_attach = begin_pass_info.color_attachment_[begin_pass_info.color_attach_count_++];
                    assert(begin_pass_info.color_attach_count_ <= MAX_RENDER_TARGET_ATTACHMENTS);
                    color_attach.texture_ = xx;
                }
            });
            RENDER_GPU_EVENT_BEGIN(nullptr, context, "RenderPass:[{}] prologue transition", pass->GetName());
            context->Enqueue(&begin_pass_info);
            RENDER_GPU_EVENT_END(nullptr, context);
        }

        void RenderGraphExecutor::ExecutePass(HAL::HALCommandContext* context, RenderPass::Handle pass)
        {
            RENDER_GPU_EVENT_BEGIN(nullptr, context, "RenderPass:[{}] @ queue[{}]", pass->GetName(). pass->GetPipeline());
            pass->Execute(context);
            RENDER_GPU_EVENT_END(nullptr, context);
        }

        void RenderGraphExecutor::ExecutePassEpilogue(HAL::HALCommandContext* context, RenderPass::Handle pass)
        {
            HAL::HALEndRenderPassPacket end_pass_info;
            context->Enqueue(&end_pass_info);
            auto epilogue_barrier = pass->GetEpilogueBarrier();
            if (!epilogue_barrier.IsEmpty()) {

            }
            RENDER_GPU_EVENT_BEGIN(nullptr, context, "RenderPass:[{}] epilogure barrier", pass->GetName());
            RENDER_GPU_EVENT_END(nullptr, context);
        }

        RenderGraphExecutor& RenderGraphExecutor::QueueExtractedTexture(Field& field, RenderTexture*& extracted_texture)
        {
            assert(field.IsOutput() && "extracted field should be output");
            auto texture_handle = GetOrCreateTexture(field);
            auto& texture = 0;
            extracted_texture->SetHAL();
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
                //begin texture HAL
                texture->SetHAL();
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
                    auto epilogue_pass = combined_nodes.back().pass_;
                    executor.InsertCallBack(epilogue_pass, [&](RenderGraphExecutor& executor) {
                        auto& pass = executor.GetRenderPass(epilogue_pass);
                        auto texture = executor.GetRenderTexture(texture_handle);
                        //end texture HAL
                        texture->EndHAL();
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

    }
}