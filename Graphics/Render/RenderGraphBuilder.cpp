#include "Graphics/Render/RenderGraphBuilder.h"
#include "Graphics/HAL/HALGlobalEntity.h"
#include "Utils/DirectedAcyclicGraph.h"
#include "Utils/SimpleCoro.h"

namespace Shard
{
    namespace Render
    {
        struct MemorySlice
        {
            uint32_t    id_;
            //uint32_t    flags_;
            uint32_t    bucket_id_;
            uint16_t    begin_time_{ 0u }; //dependency level begin
            uint16_t    end_time_{ 0u }; //dependency level end
            uint64_t    offset_{ 0u };
            uint64_t    size_{ 0u };
        };

        static inline bool IsMemorySiceOverlap(const MemorySlice& lhs, const MemorySlice& rhs) {
            return (rhs.begin_time_ > lhs.begin_time_ && rhs.begin_time_ < lhs.end_time_) ||
                (rhs.begin_time_ < lhs.begin_time_ && rhs.end_time_ > lhs.end_time_) ||
                (rhs.end_time_ > lhs.end_time_ && rhs.end_time_ < lhs.end_time_);
        }

        struct MemoryRegionPoint
        {
            uint64_t    offset_ : 63;
            uint64_t    is_end_ : 1;
        };

        struct MemoryAliasBucket
        {
            uint64_t    size_;
            uint32_t    bucket_id_:31;
            uint32_t    be_aliased_ : 1{0u};//whether bucket has only one resource
            HAL::HALAllocation* allocation_{ nullptr };
        };

        class MemoryAliasCollector
        {
        public:
            /*enqueue new slice as size descend order*/
            void Enqueue(uint32_t slice_id, uint32_t flags, uint16_t begin_time, uint16_t end_time, uint64_t size) {
                MemorySlice slice{ .id_ = slice_id, .begin_time_ = begin_time, .end_time_ = end_time, .size_ = size };
                auto position = eastl::upper_bound(slices_.begin(), slices_.end(), slice, [&](auto curr_slice) { return curr_slice.size_ < slice.size_; });
                slices_.insert(position, slice);
            }
            void DoAnalyse() {
                for (auto slice_index = 0u; slice_index < slices_.size(); ++slice_index) {
                    bool fit_result = false;
                    auto& slice = slices_[slice_index];

                    for (auto bucket_index = 0u; bucket_index < buckets_.size(); ++bucket_index) {
                        if (fit_result = FitSliceInBucket(slice, buckets_[bucket_index], bucket_slices_[bucket_index])) {
                            bucket_slices_[bucket_index].emplace_back(slice_index);
                            break;
                        }
                    }
                    if (!fit_result) {
                        AllocNewBucketForSlice(slice, slice_index);
                    }
                }
            }
            const Vector<MemoryAliasBucket>& GetBuckets() { return buckets_; }
            const MemoryAliasBucket& GetBucketForSlice(const uint32_t slice_id, uint32_t& offset) {
                //todo avoid search
                auto iter = eastl::find_if(slices_.cbegin(), slices_.cend(), [slice_id](auto& sl) {return sl.id_ == slice_id; });
                assert(iter != slices_.end());
                offset = iter->offset_;
                return buckets_[iter->bucket_id_];
            }
        private:
            void AllocNewBucketForSlice(MemorySlice& slice, uint32_t slice_index) {
                slice.bucket_id_ = buckets_.size();
                slice.offset_ = 0u;
                buckets_.emplace_back(MemoryAliasBucket{ slice.bucket_id_, (uint32_t)buckets_.size(), 0u, nullptr });
                bucket_slices_.push_back({ slice_index });
            }
            bool FitSliceInBucket(MemorySlice& slice, MemoryAliasBucket& bucket, SmallVector<uint32_t>& bucket_slices)
            {
                uint64_t best_region_gap = std::numeric_limits<uint64_t>::max();
                SmallVector<MemoryRegionPoint> region_points;
                GatherNonAliasAbleRegionPoints(region_points, slice, bucket_slices, bucket.size_);
                for (auto n = 0u; n < region_points.size() - 1u; ++n) {
                    if (region_points[n].is_end_ && !region_points[n + 1].is_end_) {
                        const auto region_gap = region_points[n + 1].offset_ - region_points[n].offset_;
                        if (region_gap > slice.size_ && region_gap < best_region_gap) {
                            slice.offset_ = region_points[n].offset_;
                        }
                    }
                }
                if (best_region_gap != std::numeric_limits<uint64_t>::max()) {
                    slice.bucket_id_ = bucket.bucket_id_;
                    bucket.be_aliased_ = 1u; //be aliased by more than one resource
                    //bucket_slices.emplace_back(slice.id_);
                    return true;
                }
                return false;
            }
            void GatherNonAliasAbleRegionPoints(SmallVector<MemoryRegionPoint>& region_points, const MemorySlice& curr_slice, const SmallVector<uint32_t>& slices, uint64_t bucket_size)
            {
                region_points.emplace_back(MemoryRegionPoint{ 0u, 1u }); //offset 0 as a fake end point
                region_points.emplace_back(MemoryRegionPoint{ bucket_size, 0u }); //offset bucket_size as a fake begin point
                for (const auto slice_id : slices) {
                    const auto& slice = slices_[slice_id];
                    if (IsMemorySiceOverlap(slice, curr_slice)) { //collect all time conflict regions
                        region_points.emplace_back(MemoryRegionPoint(slice.offset_, 0u));
                        region_points.emplace_back(MemoryRegionPoint(slice.offset_ + slice.size_, 1u));
                    }
                }
                //sort region points ascend order
                eastl::sort(region_points.begin(), region_points.end(), [](const auto& lhs, const auto& rhs) { return lhs.offset_ < rhs.offset_; });
            }
        private:
            Vector<MemoryAliasBucket>   buckets_;
            Vector<MemorySlice> slices_;
            Vector<SmallVector<uint32_t>>  bucket_slices_;
        };

        template<typename Container, typename Function>
        static void EnumerateContainer(Container& entities, Function&& func) {
            for (auto entity : entities) {
                func(entity);
            }
            //eastl::for_each(entities.begin(), entities.end(), func);
        }

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
            graph_builder.InsertOrderedAutoResolvePasses();

            //assign execute order to ordered passes
            graph_builder.AssignOrderedPassExecuteOrder();
            //hal resource allocate part
            graph_builder.AnalyseResourceUsage(resource_cache.get());
            if (GET_PARAM_TYPE_VAL(BOOL, RENDER_REORDER_PASSES)) {
                graph_builder.ReSortOrderredPasses();
                graph_builder.AnalysePassResourceTransition(resource_cache.get());
            }
            else
            {
                graph_builder.AnalyseDependencyLevelResourceTransition(resource_cache.get());
            }

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
         * as: what is the latest pass in the list of already scheduled passes which we need to wait for
         *    
         */
        void RenderGraphBuilder::ReSortOrderredPasses()
        {
            //orderred pass is first orderred by dependency level
            eastl::sort(ordered_passes_.begin(), ordered_passes_.end(), [](auto lhs, auto rhs) { return lhs->GetDependencyLevel() < rhs->GetDependencyLevel(); });
            const auto max_level = ordered_passes_.back()->DependencyLevel();
            //resort passes with the same level
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
        }

        void RenderGraphBuilder::MergeOrderedPasses(RenderResourceCache* cache)
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
            if (GET_PARAM_TYPE_VAL(BOOL, RENDER_TRANSIENT_RESOURCE)) {
                AnalyseResourceResidencyWithTransient(cache);
            }
            else{
                AnalyseResourceResidencyWithoutTransient(cache);
            }
            AnalyseResourceTransition(cache);
        }

        void RenderGraphBuilder::AnalyseResourceResidencyWithTransient(RenderResourceCache* cache)
        {
            //to avoid residency manager buffer-image ganuarity bug, allocate buffer, image seperate
            MemoryAliasCollector texture_alias_collector, buffer_alias_collector;
       
            cache->EnumerateTexture([&](auto texture_handle) {
                if (texture_handle->IsTransient()) {
                    uint32_t begin_time{ ~0u }, end_time{ 0u };
                    texture_handle->CalcLifeTime(begin_time, end_time);
                    texture_alias_collector.Enqueue((uint32_t)texture_handle, begin_time, end_time, texture_handle->GetOccupySize());
                }
            });

            cache->EnumerateBuffer([&](auto buffer_handle) {
                if (buffer_handle->IsTransient()) {
                    uint32_t begin_time{ ~0u }, end_time{ 0u };
                    buffer_handle->CalcLifeTime(begin_time, end_time);
                    buffer_alias_collector.Enqueue((uint32_t)buffer_handle, begin_time, end_time, buffer_handle->GetOccupySize());
                }
            });

            //todo 
            Utils::Schedule([&texture_alias_collector](){ texture_alias_collector.DoAnalyse(); });
            Utils::Schedule([&buffer_alias_collector](){ buffer_alias_collector.DoAnalyse(); });

            //todo do defrag here first, the allocate for current resource

            auto mem_allocator = HAL::HALGlobalEntity::Instance()->GetOrCreateMemoryResidencyManager();
            EnumerateContainer(texture_alias_collector.GetBuckets(), [](auto bucket) {
                //allocate memory
                HAL::HALAllocationCreateInfo alloc_info{
                    .type_ = HAL::EHALMemoryUsageBits::eUnkown,
                    .size_ = bucket.size_,
                    .alignment_ = 0u 
                };
                bucket.allocation_ = mem_allocator->AllocMemory(alloc_info);
            });
            EnumerateContainer(buffer_alias_collector.GetBuckets(), [](auto bucket) {
                //todo
            });
            cache->EnumerateTexture([&](auto texture_handle) {
                if (texture_handle->IsTransient()) {
                    uint32_t offset{ 0u };
                    auto bucket = alias_collector.GetBucketForSlice((uint32_t)texture_handle, offset);
                    mem_allocator->MakeTextureResident(texture_handle->GetHAL(), bucket.allocation_, offset);
                    if (bucket.is_aliased_) {
                        //todo add discard barrier 
                    }
                }
                else if(!texture_handle->IsExternal())
                {
                    //non-transient texture memory
                    auto alloc_info = mem_allocator->GetTextureResidencyInfo(texture_handle->GetHAL());
                    alloc_info.MakeDedicated();
                    //todo allocate resource backmemory
                    mem_allocator->MakeTextureResident(texture_handle->GetHAL(), nullptr);
                }
            });

            cache->EnumerateBuffer([&](auto buffer_handle) {/*todo*/});
        }

        void RenderGraphBuilder::AnalyseResourceResidencyWithoutTransient(RenderResourceCache* cache)
        {
        }

        /*gfx queue and async compute queue*/
        static constexpr uint32_t MAX_QUEUE_COUNT = 2;
     
        struct TextureIntermediateState
        {
            RenderPass::Handle  prologue_pass_[MAX_QUEUE_COUNT];
            RenderPass::Handle  epilogue_pass_[MAX_QUEUE_COUNT];
            EPipelineStageFlags prologure_stage_;
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

        static inline bool IsTransitionSupportedByQueue(EAccessFlags prev_state, EAccessFlags next_state, uint32_t queue_index) {
            auto queue_type = HAL::HALGlobalEntity::Instance()->GetQueueType(queue_index);
            if (queue_type == EPipeline::eGFX) {
                //gfx queue support all state
                return true; // IsAccessFlagsTransitionSupportedByGFXQueue(prev_state | next_state);
            }
            else if (queue_type == EPipeline::eAsyncCompute) {
                return IsAccessFlagsTransitionSupportedByComputeQueue(prev_state | next_state);
            }
            return false;
        }

        static inline uint32_t GetMostCompetentQueueIndex() {
            for (auto n = 0u; n < HAL::HALGlobalEntity::Instance()->GetQueueCount(); ++n) {
                if (HAL::HALGlobalEntity::Instance()->GetQueueType(n) == EPipeline::eGFX) {
                    return n;
                }
            }
            assert(0 && "there must be at least one gfx queue");
            return -1;
        }

        void RenderGraphBuilder::AnalysePassResourceTransition(RenderResourceCache* cache)
        {
            Map<RenderTexture::Handle, TextureIntermediateState> texture_states_ping;
            Map<RenderTexture::Handle, TextureIntermediateState> texture_states_pong;

            for (auto pass : ordered_passes_) {
                const auto queue_index = pass->QueueIndex(); //todo pipline not the same as queue
                pass->GetScheduleContext().Enumerate([&](auto& field) {
                    if (field.GetType() == Field::EType::eTexture) {
                        auto texture_handle = cache->GetOrCreateTexture(field.GetHashName());
                        if (texture_states_pong.find(texture_handle) == texture_states_pong.end()) {
                            TextureIntermediateState curr_state:
                            curr_state.prologure_stage_ = field.GetStage(); //set stage to prologue stage
                            InitInterStateFromField(curr_state.state_, field);
                            curr_state.prologue_pass_[pipeline] = curr_state.epilogue_pass_[pipeline] = pass;
                            texture_states_pong.emplace(texture_handle, std::move(curr_state));
                            continue;
                        }

                        if (!IsInterStateMergeAble(texture_states_pong[texture_handle].state_, field)) {

                            //add tansistion
                            if (auto iter = texture_states_ping.find(texture_handle); iter != texture_states_ping.end()) {
                                auto& inter_state = iter->second;
                                if (IsTextureAccessTransitionImplictAllowed(inter_state, curr_state)) {
                                    LOG(INFO) << fmt::format("implictly transition for texture{} enabled", texture_handle);
                                }
                                else
                                {
                                    /*check whether two pass are not adjacement*/
                                    const bool is_barrier_split = GET_PARAM_TYPE_VAL(BOOL, RENDER_BARRIER_SPLIT) &&
                                        (pass->GlobalExecuteOrder() - inner_state->epilogue_pass_->GlobalExecuteOrder()) > 1;
                                    Transition transition_info{};
                                    if (is_barrier_split && IsTransitionSupportedByQueue(0, 0, inner_state->epilogue_pass_->QueueIndex())
                                        && IsTransitionSupportedByQueue(0, 0, pass->QueueIndex())) {
                                        //add split barrier
                                        inter_state->epilogue_pass_->GetEpilogueBarrier().AddTransition(transition_info);
                                        pass->GetPrologureBarrier()->AddTransition(transition_info);
                                    }
                                    else {
                                        if (IsTransitionSupportedByQueue(0, 0, inner_state->epilogue_pass_->QueueIndex()))
                                        {
                                            inter_state->epilogue_pass_->GetEpilogueBarrier().AddTransition(transition_info);
                                        }
                                        else
                                        {
                                            assert(IsTransitionSupportedByQueue(0, 0, pass->Queue_index) && "both pass do not support transition is impossible");
                                            pass->GetPrologureBarrier()->AddTransition(transition_info);
                                        }
                                    }
                                }
                            }
                            std::swap(texture_states_ping[texture_handle], texture_states_pong[texture_handle]);
                            TextureIntermediateState curr_state{};
                            curr_state.prologue_pass_[pipeline] = curr_state.epilogue_pass_[pipeline] = pass;
                            curr_state.prologure_stage_ = field.GetStage();
                            InitInterStateFromField(curr_state.state_, field);
                            texture_states_pong[texture_handle] = std::move(curr_state);
                        }
                        else
                        {
                            if (!RenderPass::PassRepoInstance().IsValid(texture_states_pong[texture_handle].prologue_pass_[pipeline])) {
                                texture_states_pong[texture_handle].prologue_pass_[pipeline] = pass;
                            }
                            texture_states_pong[texture_handle].epilogue_pass_[pipeline] = pass;
                            MergeInterState(texture_states_pong[texture_handle].state_, field);
                        }
                    }
                });
            }
        }

        /**
         * \see https://medium.com/gitconnected/organizing-gpu-work-with-directed-acyclic-graphs-f3fd5f2c2af3
         * \brief as above blog says: "move all transitions into a single rerouted command list as proposed earlier." is better than reorder passes
         * for that too complex sync will be very slow
         */
        void RenderGraphBuilder::AnalyseDependencyLevelResourceTransition(RenderResourceCache* cache)
        {
            //orderred pass is first orderred by dependency level
            eastl::sort(ordered_passes_.begin(), ordered_passes_.end(), [](auto lhs, auto rhs) { return lhs->GetDependencyLevel() < rhs->GetDependencyLevel(); });
            const auto max_level = ordered_passes_.back()->DependencyLevel();

            Map<RenderTexture::Handle, TextureIntermediateState> texture_states_ping;
            Map<RenderTexture::Handle, TextureIntermediateState> texture_states_pong;

            BitSet<MAX_QUEUE_COUNT> cross_queue_sync_mask;
            for (auto level = 1u; level < max_level; ++level) {
                cross_queue_sync_mask.reset(); //no cross queu sync here
                auto compare_level = [](auto level, auto pass) { return pass->DependencyLevel() < level; };
                auto [level_begin_iter, level_end_iter] = eastl::equal_range(ordered_passes_.begin(), ordered_passes_.end(), level, compare_level);
                //gather set of queues involved in multi-queue resource read/or queues to which move state transition due to incompatiblities
                for (auto iter = level_begin_iter; iter != level_end_iter; ++iter) {
                    
                }
                if (cross_queue_sync_mask.any()) {
                    /*
                     *move transitions out into a separate command list on the most competent queue(even if most
                     *competent queue does not actually read this resource); when cross-queue dependencies take place
                     *in a single dependency level, it looks like moving transitions into a single command 
                     *list is a better approach than alternatives
                     */
                    auto resolve_barrier_pass = RenderPass::PassRepoInstance().Alloc<PrologueRenderPass>(fmt::format("DependencyLevel{}ProloguePass", level).c_str());
                    auto& resolve_barrier_batch = resolve_barrier_pass->GetPrologureBarrier();
                    for (auto iter = level_begin_iter; iter != level_end_iter; ++iter) {
                        //merge barrier batch and add sync to resolve_barrier_pass
                    }
                    ordered_passes_.insert(level_begin_iter, resolve_barrier_pass);
                }
            }
        }

        void RenderGraphBuilder::AnalyseCrossQueueSync()
        {
            Array<Array<uint32_t, MAX_QUEUE_COUNT>, MAX_QUEUE_COUNT> ssis{ 0u };
            for (auto& pass : ordered_passes_) {
                SmallVector<uint32_t> cross_queue_sync;
                const auto queue_index = pass->QueueIndex();
                const auto order_in_queue = pass->QueueExecuteOrder();
                ssis[queue_index][queue_index] = order_in_queue;
                const auto& graph_node = graph_.GetNode(graph_.GetPassIndex(pass));
                for (auto n = 0; n < graph_node->GetInEdgeCount(); ++n) {
                    const auto& edge = graph_.GetEdge(graph_node->GetInEdge(n));
                    assert(edge->GetDst() == graph_.GetPassIndex(pass) && "edge dst is curr pass");
                    auto src_pass = graph_.GetPass(edge->GetSrc());
                    const auto src_queue_index = src_pass->QueueIndex();
                    //only sync with other queue
                    if (src_queue_index == queue_index) {
                        continue; 
                    }
                    const auto src_order_in_queue = src_pass->QueueExecuteOrder();
                    if (ssis[queue_index][src_queue_index] < src_order_in_queue) {
                        //refresh sync point and add sync
                        ssis[queue_index][src_queue_index] = src_order_in_queue;
                        if (auto iter = eastl::find_if(cross_queue_sync.begin(), cross_queue_sync.end(), []() { return true; }); iter != cross_queue_sync.end()) {

                        }
                        else
                        {

                        }
                    }
                }
                if (!cross_queue_sync.empty()) {
                    //todo add sync
                }
                
            }

            //the second pass for indirect sync 
        }

        void RenderGraphBuilder::AssignOrderedPassExecuteOrder()
        {
            const auto queue_count = 0u; //todo  HAL::HALGlobalEntity::Instance()->Get//
            SmallVector<uint32_t> queue_pass_order(queue_count, 0u);
            SmallVector<uint32_t> dependency_level_pass_order(graph_.MaxDependencyLevel(), 0u);
            for (auto n = 0u; n < ordered_passes_.size(); ++n) {
                auto pass = ordered_passes_[n];
                pass->GlobalExecuteOrder() = n;
                pass->DependencyLevel() = dependency_level_pass_order[pass->DependencyLevel()]++;
                pass->QueueExecuteOrder() = queue_pass_order[pass->QueueIndex()]++;
            }
        }

    }
}

