#include "Graphics/Render/RenderGraphBuilder.h"
#include "Graphics/HAL/HALGlobalEntity.h"
#include "Utils/DirectedAcyclicGraph.h"

namespace Shard
{
    namespace Render
    {
        RenderGraphExecutor::SharedPtr RenderGraphBuilder::Compile(RenderGraph& graph)
        {
            //valid compile configure
            if (GET_PARAM_TYPE_VAL(BOOL, RENDER_ASYNC_COMPUTE) && !HAL::HALGlobalEntity::Instance()->IsAsyncComputeSupported()) {
                LOG(WARNING) << "async compute is not supportred by backend, we turn it off";
                SET_PARAM_TYPE_VAL(BOOL, RENDER_ASYNC_COMPUTE, false);
            }
            
            if (!graph.ValidateGraph()) {
                LOG(ERROR) << "render graph is corruptted";
                return {};
            }
            RenderGraphBuilder graph_builder(graph);
            auto resource_cache = std::make_unique<RenderResourceCache>();

            //pass logic part
            graph_builder.CullUnusedPasses(resource_cache.get());
            graph_builder.GenerateOrderredPasses();
            //for mergeable passes should not resort, so do merging earlier
            graph_builder.MergeOrderedPasses();
            graph_builder.ReSortOrderredPasses();
            graph_builder.InsertOrderedAutoResolvePasses();

            //hal resource allocate part
            graph_builder.AnalyseResourceUsage(resource_cache.get());
            graph_builder.AnalyseResourceTransition(resource_cache.get());

            auto graph_exe = std::make_shared<RenderGraphExecutor>();
            std::swap(graph_exe->resource_cache_, resource_cache);
            std::swap(graph_exe->ordered_passes_, graph_builder.ordered_passes_);
            return graph_exe;
        }

        void RenderGraphBuilder::CullUnusedPasses(RenderResourceCache* cache)
        {
            //track outputs related passes
            for (auto n = 0; n < graph_.OutputNum(); ++n) {
                auto& output = graph_.GetOutput(n);
                Utils::DirectGraphVisitor<RenderGraph, Utils::DFSSearch> visitor(graph_, graph_.GetNode(graph_.GetPassIndex(output.pass_)));
                while (true) {
                    auto pass_node = visitor.Next();
                    if (nullptr == pass_node) {
                        break;
                    }
                    //set pass alive
                    pass_node->Evict(false);
                }
            }
            
            //remove noused passes
            Utils::DirectGraphVisitor<RenderGraph, Utils::DFSSearch> visitor(graph_, graph_.GetNode(0u));
            while (true) {
                auto pass_node = visitor.Next();
                if (nullptr == pass_node) {
                    break;
                }
                if (pass_node->IsEvicted()) {
                    graph_.RemovePass(graph_.GetPass(pass_node->GetIndex()));
                }
            }
        }

        void RenderGraphBuilder::GenerateOrderredPasses()
        {
            graph_.Sort();
            ordered_passes_.reserve(graph_.ordered_nodes_.size());
            /*copy ordered pass to builder*/
            for (const auto node_index : graph_.ordered_nodes_) {
                ordered_passes_.push_back({ graph_.GetPass(node_index)});
            }
            LOG(INFO) << fmt::format("render graph generate ordered pass success with {} passes", ordered_passes_.size());
        }
        /**
         * \brief resort passes have the same dependency level and in the same queue
         * as: What is the latest pass in the list of already scheduled passes which we need to wait for
         *    
         */
        void RenderGraphBuilder::ReSortOrderredPasses()
        {
#ifdef RESORT_ORDER_PASS
            //resort passes with the same level£¬orderred pass is already orderred by dependency level
            const auto max_level = ordered_passes_.back()->GetDependencyLevel();
            for (auto level = 1; level <= max_level; ++level) {

                auto compare_level = [](auto level, auto pass) { return pass->GetDependencyLevel() < level; };
                auto [level_begin_iter,level_end_iter] = eastl::equal_range(ordered_passes_.begin(), ordered_passes_.end(), level, compare_level);

                auto get_dependency_least_overlap = [&](auto pass) {
                    const auto* pass_node = GetNode(GetPassIndex(pass));
                    auto pass_iter = eastl::find(ordered_passes_.begin(), level_end_iter, pass);
                    auto least_overlap = std::distance(orderred_passes_.begin(), pass_iter);
                    for (auto n = 0u; n < pass_node->GetInEdgeCount(); ++n) {
                        const auto edge_index = pass_node->GetInEdge(n);
                        const auto src_pass_index = GetEdge(edge_index)->GetSrc();
                        auto src_pass_handle = render_graph_.node_data_[src_pass_index].pass_;
                        auto src_pass_iter = eastl::find(ordered_passes_.begin(), level_begin_iter, src_pass_handle);
                        assert(src_pass_iter != level_begin_iter);
                        least_overlap = std::min(min_overlap, std::distance(src_pass_iter, pass_iter));
                    }
                    return least_overlap;
                };

                auto compare_overlap = [&](auto lhs, auto rhs) {
                    if (lhs.GetPipeline() == rhs.GetPipeline()) {
                        return get_dependency_least_overlap() > get_dependency_least_overlap(rhs);
                    }
                    return false; //different queue pass no need to resort
                };

                eastl::sort(level_begin_iter, level_end_iter, compare_overlap);
            }
#endif
        }

        void RenderGraphBuilder::MergeOrderedPasses()
        {
            Vector<RenderPass::Handle> new_ordered_passes;
            for (auto start_iter = ordered_passes_.begin(); start_iter != ordered_passes_.end(); ) {
                auto end_iter = start_iter;
                while(++end_iter != ordered_passes_.end()) {
                    bool merge_able{ true };
                    for (auto test_iter = start_iter; test_iter != end_iter; ++test_iter) {
                        if (!graph_.IsPassMergeAble(*test_iter, *end_iter)) {
                            merge_able = false;
                            break;
                        }
                    }
                    if (!merge_able) {
                        --end_iter;//fallback to last mergeable position
                        break;
                    }
                }
                if (end_iter != start_iter) {
                    //merge able pass seq found, generate mergedpass
                    auto merge_pass = RenderPass::PassRepoInstance().Alloc<PackedRenderPass>({&(*start_iter), uint32_t(end_iter-start_iter)}); 
                    new_ordered_passes.emplace_back(merge_pass);
                    start_iter = end_iter;
                }
                else
                {
                    ++start_iter;
                    new_ordered_passes.emplace_back(*start_iter);
                }
            }
            //swap the new ordered 
            std::swap(ordered_passes_, new_ordered_passes);
        }

        //todo swapchain msaa resolve
        void RenderGraphBuilder::InsertOrderedAutoResolvePasses()
        {
            for (auto iter = ordered_passes_.begin(); iter != ordered_passes_.end(); ++iter) {
                auto& schedule_context = (*iter)->GetScheduleContext();
                schedule_context.Enumerate([&](auto& field) {
                    if (field.IsInput()) {
                        const auto& parent = field.GetParentName();
                        //resolve msaa
                        if (field.GetSampleCount()==1u && parent.GetSampleCount()>1u)
                        {
                            auto resovle_handle = RenderPass::PassRepoInstance().Alloc<TextureBlitRenderPass>();
                            resovle_handle->Init(); //init it out of rendergraph
                            ordered_passes_.insert(iter, resolve_handle);
                            //texture blit render pass only has two field
                            resolve_handle->GetScheduleContext().Enumerate([&]()(auto & resovle_field){
                                if (resovle_field.IsOuput()) {
                                    field.ParentAs(resovle_field.GetName());
                                }
                                else
                                {
                                    resovle_field.ParentAs(parent.GetName());
                                }
                            });
                        }
                    }
                });
            }
        }

        void RenderGraphBuilder::AnalyseResourceUsage(RenderResourceCache* cache)
        {
            for (auto pass : ordered_passes_) {
                const auto generate_output_resource = [&, this](const Field& field) {
                    if (field.IsOutput()) {
                        if (field.GetType() == Field::EType::eTexture) {
                            auto texture_handle = cache->GetOrCreateTexture(field.GetHashName());
                            texture_handle->Write(pass);
                        }
                        else if (field.GetType() == Field::EType::eBuffer)
                        {
                            auto buffer_handle = cache->GetOrCreateBuffer(field.GetHashName());
                            buffer_handle->Write(pass);
                        }
                    }
                };
                const auto hook_input_resource = [&, this](const Field& field) {
                    if (field.IsInput() && !field.IsExternal()) {
                        //todo maybe multi parent
                        const auto& parent_name = field.GetParentName();
                        if (field.GetType() == Field::EType::eTexture) {
                            auto texture_opt = cache->TryGetTexture(parent_name);
                            assert(texture_opt && "parent resource not exist");
                            auto texture_handle = texture_opt.value();
                            texture_handle->Read(pass);
                        }
                        else if (field.GetType() == Field::EType::eBuffer) {
                            auto buffer_opt = cache->TryGetBuffer(parent_name);
                            assert(buffer_opt && "parent resource not exist");
                            auto buffer_handle = cache->TryGetBuffer(parent_name).value();
                            buffer_handle->Read(pass);
                        }
                    }
                };
                pass->GetScheduleContext().Enumerate(generate_output_resource).Enumerate(hook_input_resource);
            }
        }

        void RenderGraphBuilder::AnalyseResourceHALMemoryUsage(RenderResourceCache* cache)
        {
            //allocate hal resource
            cache->EnumerateTexture([&](auto texture_handle) {
                texture_handle->SetHAL();
            });
            AnalyseResourceTrasientResidency(cache);
            AnalyseResourceTransition(cache);
        }

        void RenderGraphBuilder::AnalyseResourceTrasientResidency(RenderResourceCache* cache)
        {
            //todo
        }

        struct TextureIntermediateState
        {
            RenderPass::Handle  prologue_pass_[MAX_QUEUE_COUNT];
            RenderPass::Handle  epilogue_pass_[MAX_QUEUE_COUNT];
            SmallVector<TextureState>   state_;
        };

        static inline void InitInterStateFromField(SmallVector<TextureState>& state, const Field& curr_field) {
            assert(state.empty());
            if (curr_field.IsWholeResource()) {
                state.push_back(curr_field.GetSubFieldState());
            }
            else
            {
                const auto texture_range = TextureSubRangeRegion::FromDims(curr_field.GetDimension());
                state.resize(texture_range.GetSubRangeIndexCount());
                //curr_field.GetBufferSubRange();
            }
        }
        static inline bool IsInterStateMergeAble(const SmallVector<TextureState>& state, const Field& curr_field) {
            const auto& curr_state = curr_field.GetSubFieldState();
            if (curr_field.IsWholeResource() && state.size() == 1u)
            {
                return IsTextureStateMergeAllowed(state.front(), curr_state);
            }
            else
            {
                if (curr_field.IsWholeResource()) {
                    //to do not support
                }
                else
                {
                    
                }
            }
            return true;
        }

        static void MergeInterState(SmallVector<TextureState>& state, const Field& curr_field) {
            //curr_field.GetTextureSubRange();
            const auto& curr_state = curr_field.GetSubFieldState();
            if (curr_field.IsWholeResource() && state.size() == 1u) {
                state[0] = MergeState(state[0], curr_state);
            }
            else
            {

            }

        }

        void RenderGraphBuilder::AnalyseResourceTransition(RenderResourceCache* cache)
        {
            Map<RenderTexture::Handle, TextureIntermediateState> texture_states_ping;
            Map<RenderTexture::Handle, TextureIntermediateState> texture_states_pong;

            for (auto pass : ordered_passes_) {
                pass->GetScheduleContext().Enumerate([&](auto& field) {
                    if (field.GetType() == Field::EType::eTexture) {
                        auto texture_handle = cache->GetOrCreateTexture(field.GetHashName());
                        if (texture_states_pong.find(texture_handle) == texture_states_pong.end()) {
                            TextureIntermediateState curr_state:
                            curr_state.prologue_pass_ = pass;
                            curr_state.epilogue_pass_ = pass;
                            InitStateFromField(curr_state.state_, field);
                            texture_states_pong.emplace(texture_handle, std::move(curr_state));
                            continue;
                        }

                        if (!IsInterStateMergeAble(texture_states_pong[texture_handle].state_, field)) {

                            //add tansistion
                            if (auto iter = texture_states_ping.find(texture_handle); iter != texture_states_ping.end()) {
                                auto& inter_state = iter->second;
                                if (GET_PARAM_TYPE_VAL(BOOL, RENDER_BARRIER_SPLIT)) {
                                    //add split barrier
                                    Transition transition_info{};
                                    inter_state->epilogue_pass_->GetEpilogueBarrier().AddTransition(transition_info);
                                    pass->GetPrologureBarrier()->AddTransition(transition_info);
                                }
                                else
                                {
                                    Transition transition_info{};
                                    inter_state->epilogue_pass_->GetEpilogueBarrier().AddTransition(transition_info);
                                }
                            }
                            std::swap(texture_states_ping[texture_handle], texture_states_pong[texture_handle]);
                            TextureIntermediateState curr_state{};
                            curr_state.prologue_pass_ = pass;
                            curr_state.epilogue_pass_ = pass;
                            InitInterStateFromField(curr_state.state_, field);
                            texture_states_pong[texture_handle] = std::move(curr_state);
                        }
                        else
                        {
                            texture_states_pong[texture_handle].epilogue_pass_ = pass;
                            MergeInterState(texture_states_pong[texture_handle].state_, field);
                        }
                    }
                });
            }
        }

    }
}

