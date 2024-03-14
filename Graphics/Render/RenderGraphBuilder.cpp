#include "Graphics/Render/RenderGraphBuilder.h"
#include "Graphics/RHI/RHIGlobalEntity.h"
#include "Utils/DirectedAcyclicGraph.h"

namespace Shard
{
    namespace Render
    {

        void RenderGraphBuilder::AnalysisResourceUsage()
        {
            /*all resource field manager buffer*/
            Map<String, FieldResourcePlanner> resource_planers;

            for (auto& [pass_handle, pass] : graph_exe_->passes_) {
                auto& fields = pass.GetScheduleContext().GetFields();
                for (auto& [_, field] : fields) {
                    /*render graph do not dist external/unreferenced resource*/
                    if (field.IsExternal()||!field.IsReferenced()) {
                        continue;
                    }
                    auto& parent_name = field.GetParentName();
                    if (resource_planers.find(parent_name) == resource_planers.end()) {
                        resource_planers.insert(eastl::make_pair(parent_name, FieldResourcePlanner()));
                    }

                    auto& planner = resource_planers[parent_name];
                    if (field.IsOutput()) {
                        planner.AppendProducer(field, pass_handle);
                    }
                    else
                    {
                        planner.ApeendConsumer(field, pass_handle);
                    }
                }
            }

            //do all resource planer work
            for (auto& [_, planner] : resource_planers) {
                planner.DoResourcePlan(*graph_exe_);
            }

        }

        void RenderGraphBuilder::Compile(const BuildConfig& build_param)
        {
            if (!IsReadyToBake()) {
                return; //todo check whether params are the same
            }

            //pre valid build param
            const auto valid_build_config = [](const auto& build_param)
            {
                BuildConfig param = build_param;
                if (param.aync_enable_) {
                    param.aync_enable_ = RHI::RHIGlobalEntity::Instance()->IsAsyncComputeSupported();
                    LOG(WARNING) << "aync compute is not supported by device, so closed";
                }
                if (param.hw_raytrace_enable_) {
                    param.hw_raytrace_enable_ = RHI::RHIGlobalEntity::Instance()->IsHWRayTraceSupported();
                    LOG(WARNING) << "hardware ray tracing is not supported by device, so closed";
                }
                return param;
            };

            const auto param = valid_build_config(build_param);

            //1.step culling no use pass
            if (param.culling_passes_) {
                CullingUnusedPasses();
            }

            const bool is_async_compute_enable = !!param.aync_enable_;
            //2.step analysis resource transistant; add barriers
            for (auto& edge : graph_.) //?
            {
            }


            //3.valid the finalize graph
            if (!graph_.IsValid())
            {
                LOG(ERROR) << "RenderGraph Compile failed";
            }

        }

        RenderGraphExecutor::SharedPtr RenderGraphBuilder::Bake()
        {
            //4. do resource analysis
            //last step copy execute list as ascend ordered
            //std::sort(builder.command_list_.begin(), builder.command_list_.end());
            auto graph_exe = std::make_shared<RenderGraphExecutor>();
            graph_exe->Bake()
            return graph_exe;
        }

        bool RenderGraphBuilder::IsReadyToBake()const
        {
            return !graph_.IsCompileNeeded();
        }

        void RenderGraphBuilder::CullingNoUsePasses()
        {
            //track outputs related passes
            for (auto n = 0; n < graph_.OutputNum(); ++n) {
                Utils::DirectGraphVisitor<decltype(graph_), Utils::DFSSearch> visitor(graph_, graph_.GetNode(0u));
                while (true) {
                    auto pass_node = visitor.Next();
                    if (nullptr == pass_node) {
                        break;
                    }
                    //set pass valid for 
                    pass_node->SetFlags(NodeData::EFlags::eValid);
                }
            }
            
            //remove noused passes

            
        }

        void RenderGraphBuilder::MergeOrderedPasses()
        {
            Vector<RenderPass::Handle> new_ordered_passes;
            for (auto start_iter = graph_.ordered_pass_.begin(); start_iter != graph_.ordered_pass_.end(); ) {
                auto end_iter = start_iter;
                while(++end_iter != graph_.ordered_pass_.end()) {
                    bool merge_able{ true };
                    for (auto test_iter = start_iter; test_iter != end_iter; ++test_iter) {
                        if (!graph_.IsPassMergeable(*test_iter, *end_iter)) {
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
                    auto merge_pass = RenderPass::PassRepoInstance().Alloc<PackedRenderPass>(xx); //todo
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
            std::swap(graph_.ordered_pass_, new_ordered_passes);
        }

    }
}

