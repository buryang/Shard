#include "Render/RenderGraph.h"
//https://levelup.gitconnected.com/organizing-gpu-work-with-directed-acyclic-graphs-f3fd5f2c2af3

namespace Shard
{
    namespace Render
    {
        void RenderGraph::AddPass(RenderPass::Handle pass)
        {
            //check pass schedule context is valid
            if (pass->GetScheduleContext().IsValid()) {
                if (pass_to_index_.find(pass) != pass_to_index_.end()) {
                    const auto node_index = AddNode();
                    pass_to_index_[pass] = node_index;
                    node_data_[node_index] = { pass };
                    pass->Init(); //setup pass
                }
                else
                {
                    LOG(FATAL) << fmt::format("render graph failed to add pass<{}>", pass->GetName());
                }
            }
            else
            {
                LOG(FATAL) << fmt::format("render pass:[{}] schedule context is not valid", pass->GetName());
            }
        }
        void RenderGraph::RemovePass(const RenderPass::Handle pass)
        {
            if (auto iter = pass_to_index_.find(pass); iter != pass_to_index_.end()) {
                auto pass_index = iter->second;
                RemoveNode(pass_index);
                pass_to_index_.erase(pass);
                node_data_.erase(pass_index);
                RenderPass::PassRepoInstance().Free(pass);
            }
        }

        bool RenderGraph::ConnectPass(RenderPass::Handle producer, RenderPass::Handle consumer)
        {
            auto& producer_context = producer->GetScheduleContext();
            auto& consumer_context = consumer->GetScheduleContext();
            bool success_to_connect{ false };
            producer_context.Enumerate([&, this](auto& output_field) {
                if (!output_field.IsOutput()) return;
                consumer_context.Enumerate([&, this](auto& input_field) {
                    if (!input_field.IsInput()) return;
                    if (ConnectField(output_field, input_field)) {
                        success_to_connect = true;
                    }
                });
            });
            return success_to_connect;
        }

        void RenderGraph::DisConnectPass(RenderPass::Handle producer, RenderPass::Handle consumer)
        {
            const auto edge_index = GetEdgeIndex(pass_to_index_[producer], pass_to_index_[consumer]);
            if (edge_index != -1) {
                RemoveEdge(edge_index);
                edge_data_.erase(edge_index);
            }
            LOG(WARNING) << fmt::format("pass[{}] is not connected to pass[{}]", consumer->GetName(), producer->GetName());
        }
        
        bool RenderGraph::ConnectField(RenderPass::Handle producer, Field& src, RenderPass::Handle consumer, Field& dst)
        {
            const auto edge_index = GetEdgeIndex(GetPassIndex(producer), GetPassIndex(consumer));
            if (edge_index == -1) {
                auto new_edge_index = AddEdge(GetPassIndex(producer), GetPassIndex(consumer));
                edge_data_[new_edge_index].ref_count_ = 1u;
            }
            else
            {
                edge_data_[edge_index].ref_count_++;
            }
            dst.ParentName(src.GetHashName());
        }

        void RenderGraph::DisConnectField(RenderPass::Handle producer, Field& src, RenderPass::Handle consumer, Field& dst)
        {
            const auto edge_index = GetEdgeIndex(GetPassIndex(producer), GetPassIndex(consumer));
            assert(edge_index != -1);
            auto ref_count = --edge_data_[edge_index].ref_count_;
            if (!ref_count) {
                edge_data_.erase(edge_index);
                RemoveEdge(edge_index);
            }
            dst.ParentName(dst.GetHashName());
        }

        void RenderGraph::MarkOutput(RenderPass::Handle pass, Field& field) {
            outputs_.insert({ pass, &field });
        }

        void RenderGraph::UnMarkOutput(RenderPass::Handle pass, Field& field)
        {
            outputs_.erase({ pass, &field });
        }

        void RenderGraph::Sort()
        {
            BaseClass::TopoSort();
            //precalc longest path for dependency level 
            for (const auto node_index : ordered_nodes_) {
                const auto& node = GetNode(node_index);
                const auto node_level = node_data_[node_index].pass_->GetDependencyLevel();
                DirectGraphVisitor<BaseClass, Utils::BFSSearch> visitor(*this, node);
                for (const auto* next = visitor.Next(); next != nullptr; ) {
                    const auto next_index = next->GetIndex();
                    auto pass = node_data_[next_index].pass_;
                    if (pass->GetDependencyLevel() < node_level + 1u) {
                        pass->SetDependencyLevel(node_level + 1u);
                    }
                }
                max_dependency_level_ = std::max(max_dependency_level_, node_level);
            }
        }
        
        void RenderGraph::Serialize(Utils::IOArchive& ar)
        {
#if 0
            if (ar.IsReading())
            {
                ar << 
                size_type order_pass_num{ 0u };
                ar >> order_pass_num;
            }
            else
            {
                ar << ordered_pass_.size();
            }
#endif
        }

        /*unreal engine merge passes which are both raster pass and have the same render target.
        * while granite said:'They share some color/depth/input attachments, not more than one 
        * unique depth/stencil attachment exists'
        */
        bool RenderGraph::IsPassMergeAble(RenderPass::Handle prev, RenderPass::Handle next)const
        {
            // check they are both graphics passes. to do refactor
            if (prev->GetPipeline() == EPipeline::eAsyncCompute || next->GetPipeline() == EPipeline::eAsyncCompute) {
                return false;
            }

            //add to it by me
            //if (prev->GetDependencyLevel() != next->GetDependencyLevel()) {
            //    return false;
            //}

            {
                SmallVector<Field> prev_outputs, next_outputs;

                //they share some color/depth/input attachments
                prev->GetScheduleContext().EnumerateOutputFields(prev_outputs);
                next->GetScheduleContext().EnumerateOutputFields(next_outputs);

                //check whether both passes write to the same field
                if (prev_outputs.size() != next_outputs.size()) {
                    return false;
                }
                for (auto n = 0; n < prev_outputs.size(); ++n) {
                    //every attachment must be the same
                    if(resource_to_index_[prev_outputs[n].GetHashName()] != resource_to_index_[next_outputs[n].GetHashName()]){
                        return false;
                    }
                }
            }

            //todo their dependencies can be implemented with BY_REGION_BIT
            return true;
        }

        bool RenderGraph::ValidateGraph()
        {
            if (TestCyclicDetection()) {
                LOG(ERROR) << "render graph is cyclic one";
                return false;
            }
            
            if (TestEdgeOrphan()) {
                LOG(ERROR) << "render graph has some illegal edge";
                return false;
            }

            for (auto n = 0u; n < OutputNum(); ++n) {
                auto pass = GetOutput(n).pass_;
                auto* pass_node = GetNode(GetPassIndex(pass));
                Utils::DirectGraphVisitor<RenderGraph, Utils::DFSSearch> visitor(*this, pass_node);
                for (;;) {
                    auto& schedule_context = pass->GetScheduleContext();
                    //check
                    bool field_check_pass{ true };
                    schedule_context.Enumerate([&](auto& field) {
                        if (field_check_pass && field.IsInput()) {
                            if (!field.IsInputConnected()) {
                                LOG(ERROR) << fmt::format("render pass[{}].field[{}] not connected", pass->GetName(), field.GetName());
                                field_check_pass = false;
                            }
                        }
                    });
                    if (!field_check_pass) {
                        return false;
                    }
                    pass_node = visitor.Next();
                    if (nullptr == pass_node)
                    {
                        break;
                    }
                    pass = node_data_[pass_node->GetIndex()];
                    
                }
                
            }
            return true;
        }

}

}