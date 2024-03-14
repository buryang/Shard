#include "Render/RenderGraph.h"

namespace Shard
{
    namespace Render
    {
        void RenderGraph::AddPass(RenderPass::PassHandle pass)
        {
            if (pass_to_index_.find(pass) != pass_to_index_.end()) {
                pass_to_index_[pass] = xx; // pass_handle;
                AddNode(uint32_t(pass_handle));
            }
            else
            {
                LOG(FATAL) << fmt::format("{}:failed to add pass<{}>", __FUNCTION__, pass->GetName());
            }
        }
        void RenderGraph::RemovePass(const RenderPass::Handle pass)
        {
            if (auto iter = pass_to_index_.find(pass); iter != pass_to_index_.end()) {
                auto pass_index = iter->second;
                RemoveNode(pass_index);//FIXME how to deal edge data
                pass_to_index_.erase(pass);
                RenderPass::PassRepoInstance().Free(pass);
            }
        }
        void RenderGraph::InsertPass(RenderPass::Handle prev_pass, RenderPass::Handle pass)
        {
        }
        static Field::Name GetEdgeNameFromFieldPair(const Field::Name prev, const Field::Name next) {
            //todo
        }
        void RenderGraph::ConnectPass(RenderPass::Handle producer, RenderPass::Handle consumer, Field& src, Field& dst)
        {
            const auto& edge_name = GetEdgeNameFromFieldPair(src.GetName(), dst.GetName());
            if (edge_to_index_.find(edge_name) != edge_to_index_.end()) {
                //error fix
            }
            auto edge_index = edge_manager_.Alloc(src, dst);
            edge_to_index_[edge_name] = edge_index;
            AddEdge(uint32_t(producer), uint32_t(consumer)); //???
        }
        void RenderGraph::DisConnectField(Field& src, Field& dst)
        {
            const auto& edge_name = GetEdgeNameFromFieldPair(src.GetName(), dst.GetName());
            if (auto iter = edge_to_index_.find(edge_name); iter != edge_to_index_.end()) {
                auto edge_index = iter->second;
                RemoveEdge(edge_index);
                edge_to_index_.erase(iter);
                edge_manager_.Free(edge_index);
            }
        }
        uint32_t RenderGraph::GetPassIndex(RenderPass::Handle pass)const
        {
            auto iter = pass_to_index_.find(pass);
            if (iter != pass_to_index_.end()) {
                return iter->second;
            }
            return INVALID_INDEX;
        }
        
        void RenderGraph::Serialize(Utils::IOArchive& ar)
        {
            if (ar.IsReading())
            {

            }
            else
            {

            }
        }
        uint32_t RenderGraph::CalcPassDependencyLevel(RenderPass::Handle pass)
        {
            return 0;
        }

        //fixme if realize dependency level, shoul only check two pass in the same level
        //and the same queue
        bool RenderGraph::IsPassMergeable(RenderPass::Handle prev, RenderPass::Handle next)
        {
            // first check whether pass in the same queue
            if (prev->GetPipeline() == EPipeline::eAsyncCompute || next->GetPipeline() == EPipeline::eAsyncCompute) {
                return false;
            }
            {
                SmallVector<Field> prev_inputs, prev_outputs, next_inputs, next_outputs;

                prev->GetScheduleContext().EnumerateInputFields(prev_inputs);
                prev->GetScheduleContext().EnumerateOutputFields(prev_outputs);
                next->GetScheduleContext().EnumerateInputFields(next_inputs);
                next->GetScheduleContext().EnumerateOutputFields(next_outputs);

                //check pass resource dependency
                for (const auto& fd : next_inputs) {
                    if (auto iter = eastl::find(prev_outputs.cbegin(), prev_outputs.cend(),
                                            [&](auto& curr_field) { return resource_to_index_[curr_field.GetName()] == resource_to_index_[fd.GetName()]; }); iter != prev_outputs.cend()) {
                        return false;
                    }
                }

                //check whether both passes write to the same field
                for (const auto& fd : next_outputs) {
                    if (auto iter = eastl::find(prev_outputs.cbegin(), prev_outputs.cend(), 
                                            [&](auto& curr_field) { return resource_to_index[curr_field.GetName()] == resource_to_index_[fd.GetName()]; }); iter != prev_outputs.cend()) {
                        return false;
                    }
                }
            }

            return true;
    }
}