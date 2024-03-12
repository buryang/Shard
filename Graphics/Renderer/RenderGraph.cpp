#include "Renderer/RtRenderGraph.h"

namespace Shard
{
    namespace Renderer
    {
        RtRendererGraph::RtRendererGraph():pass_manager_(nullptr), edge_manager_(nullptr)
        {
            rm_node_clk_ = [&, this](auto node_index) {
                pass_manager_.Free(node_index);
            };
            rm_edge_clk_ = [&, this](auto edge_index) {
                edge_manager_.Free(edge_index);
            };
        }
        void RtRendererGraph::AddPass(RtRendererPass::Ptr pass, const String& pass_name)
        {
            if (pass_to_index_.find(pass_name) != pass_to_index_.end()) {
                auto pass_handle = pass_manager_.Insert(pass);
                pass_to_index_[pass_name] = pass_handle;
                AddNode(uint32_t(pass_handle));
            }
            else
            {
                LOG(FATAL) << __FUNCTION__ << ":failed to add pass<" << pass_name.c_str() << ">" << std::endl;
            }
        }
        void RtRendererGraph::RemovePass(const String& pass_name)
        {
            if (auto iter = pass_to_index_.find(pass_name); iter != pass_to_index_.end()) {
                auto pass_index = iter->second;
                RemoveNode(pass_index);//FIXME how to deal edge data
                pass_to_index_.erase(pass_name);
            }
        }
        void RtRendererGraph::ConnectPass(PassHandle producer, PassHandle consumer, RtField& src, RtField& dst)
        {
            const auto& edge_name = GetEdgeNameFromFieldPair(src.GetName(), dst.GetName());
            if (edge_to_index_.find(edge_name) != edge_to_index_.end()) {
                //error fix
            }
            auto edge_index = edge_manager_.Alloc(src, dst);
            edge_to_index_[edge_name] = edge_index;
            AddEdge(uint32_t(producer), uint32_t(consumer));
        }
        void RtRendererGraph::DisConnectPass(RtField& src, RtField& dst)
        {
            const auto& edge_name = GetEdgeNameFromFieldPair(src.GetName(), dst.GetName());
            if (auto iter = edge_to_index_.find(edge_name); iter != edge_to_index_.end()) {
                auto edge_index = iter->second;
                RemoveEdge(edge_index);
                edge_to_index_.erase(iter);
                edge_manager_.Free(edge_index);
            }
        }
        uint32_t RtRendererGraph::GetPassIndex(const String& pass_name)const
        {
            auto iter = pass_to_index_.find(pass_name);
            if (iter != pass_to_index_.end()) {
                return iter->second;
            }
            return INVALID_INDEX;
        }
        
        void RtRendererGraph::Serialize(Utils::IOArchive& ar)
        {
        }
        
        void RtRendererGraph::UnSerialize(Utils::IOArchive& ar)
        {
        }
    }
}