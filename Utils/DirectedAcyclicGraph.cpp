#include <algorithm>
#include "DirectedAcyclicGraph.h"

namespace Shard
{
    namespace Utils
    {
        void NodeData::AddInEdge(uint32_t edge)
        {
            if (eastl::find(in_edges_.begin(), in_edges_.end(), edge) == in_edges_.end() 
                && eastl::find(out_edges_.begin(), out_edges_.end(), edge) == out_edges_.end())
            {
                in_edges_.push_back(edge);
            }
        }

        void NodeData::AddOutEdge(uint32_t edge)
        {
            if (eastl::find(in_edges_.begin(), in_edges_.end(), edge) == in_edges_.end()
                && eastl::find(out_edges_.begin(), out_edges_.end(), edge) == out_edges_.end())
            {
                out_edges_.push_back(edge);
            }
        }

        void NodeData::RemoveInEdge(uint32_t edge)
        {
            if (auto iter = eastl::find( in_edges_.begin(), in_edges_.end(), edge); iter != in_edges_.end())
            {
                std::swap(*iter, in_edges_.back());
                in_edges_.pop_back();
            }
        }

        void NodeData::RemoveOutEdge(uint32_t edge)
        {
            if (auto iter = eastl::find(out_edges_.begin(), out_edges_.end(), edge); iter != out_edges_.end())
            {
                std::swap(*iter, out_edges_.back());
                out_edges_.pop_back();
            }
        }

        uint32_t DirectedAcyclicGraph::AddEdge(uint32_t src_node_index, uint32_t dst_node_index)
        {
            assert(src_node_index != dst_node_index);
            auto* src_node = GetNode(src_node_index);
            auto* dst_node = GetNode(dst_node_index);
            const auto edge_index = edges_.size();

            //check node is valid, and avoid cyclic
            if (src_node && dst_node && !IsConnected(dst_node_index, src_node_index))
            {
                edges_[edge_index] = { src_node_index, dst_node_index, edge_index };
                src_node->AddOutEdge(edge_index);
                dst_node->AddInEdge(edge_index);
                return edge_index;
            }
            return -1;
        }

        uint32_t DirectedAcyclicGraph::GetEdgeIndex(uint32_t src_node, uint32_t dst_node)
        {
            const auto* src_node_data = GetNode(src_node);
            for (auto n = 0u; n < src_node_data->GetOutEdgeCount(); ++n) {
                const auto* edge_data = GetEdge(src_node_data->GetOutEdge(n));
                if (edge_data->GetDst() == dst_node) {
                    return edge_data->GetIndex();
                }
            }
            return -1;
        }

        void DirectedAcyclicGraph::RemoveEdge(uint32_t index)
        {
            auto edge = GetEdge(index);
            if (edge != nullptr)
            {
                GetNode(edge->GetSrc())->RemoveOutEdge(index);
                GetNode(edge->GetDst())->RemoveInEdge(index);
                edges_.erase(index);
            }
        }

        uint32_t DirectedAcyclicGraph::AddNode()
        {
            const auto node_index = nodes_.size();
            nodes_.insert(eastl::make_pair(node_index, Node{ node_index }));
            return node_index;
        }

        void DirectedAcyclicGraph::RemoveNode(uint32_t node_index)
        {
            auto node = GetNode(node_index);
            if (nullptr != node)
            {
                for (auto n = 0; n < node->GetInEdgeCount(); ++n)
                {
                    auto edge_index = node->GetInEdge(n);
                    node->RemoveInEdge(edge_index);
                    RemoveEdge(edge_index);
                }

                for (auto n = 0; n < node->GetOutEdgeCount(); ++n)
                {
                    auto edge_index = node->GetOutEdge(n);
                    node->RemoveOutEdge(edge_index);
                    RemoveEdge(edge_index);
                }
                nodes_.erase(node_index);
            }
        }

        void DirectedAcyclicGraph::TopoSort()
        {
            ordered_nodes_.reserve(nodes_.size());
            Vector<bool> visited_flags(nodes_.size());
            std::function<void(uint32_t, const Node&)> dfs_traverse = [&, this](auto node_index, const auto& node) {
                if (!visited_flags[node_index]) {
                    DirectGraphVisitor<DirectedAcyclicGraph, DFSSearch> adjv(*this, node);
                    for (auto* next = adjv.Next(); next != nullptr; next = adjv.Next()) {
                        dfs_traverse(next->GetIndex(), *next);
                    }
                    visited_flags[node_index] = true;
                    ordered_nodes_.push_back(node_index);
                }
            };
            for (const auto&[node_index, node] : nodes_) {
                dfs_traverse(node_index, node);
            }

            //orderred nodes in reverse order
            eastl::reverse(ordered_nodes_.begin(), ordered_nodes_.end());
        }

        DirectedAcyclicGraph::Node* DirectedAcyclicGraph::GetNode(uint32_t node_index)
        {
            if (nodes_.find(node_index) != nodes_.end())
            {
                return &nodes_[node_index];
            }
            return nullptr;
        }

        DirectedAcyclicGraph::Edge* DirectedAcyclicGraph::GetEdge(uint32_t edge_index)
        {
            if (edges_.find(edge_index) != edges_.end())
            {
                return &edges_[edge_index];
            }
            return nullptr;
        }

        float DirectedAcyclicGraph::LongestPath(uint32_t src_node, uint32_t dst_node)const
        {
            //todo
            return 0.0f;
        }

        float DirectedAcyclicGraph::ShortestPath(uint32_t src_node, uint32_t dst_node)const
        {
            //todo
            return 0.0f;
        }

        bool DirectedAcyclicGraph::TestEdgeOrphan() const
        {
            for (const auto& [_, edge] : edges_) {
                if (!edge.IsValid()) {
                    return true;
                }
            }
            return false;
        }

        bool DirectedAcyclicGraph::TestCyclicDetection() const
        {
            //check whether graph is acyclic, ugly code now
            for (const auto& node : nodes_)
            {
                if (IsConnected(node.first, node.first)) {
                    return true;
                }
            }
            return false;
        }

        bool DirectedAcyclicGraph::IsConnected(uint32_t lhs_node, uint32_t rhs_node) const
        {
            DirectGraphVisitor<DirectedAcyclicGraph, BFSSearch> visitor(*this, const_cast<DirectedAcyclicGraph*>(this)->GetNode(lhs_node));
            do{
                const auto* next = visitor.Next();
                if (nullptr == next) {
                    break;
                }
                //check pointer 
                if (next->GetIndex() == rhs_node) {
                    return true;
                }

            } while (true);
            return false;
        }

        void DirectedAcyclicGraph::Truncate()
        {
            for (auto& [index, node] : nodes_) {
                if (!node.IsEvicted()) {
                    RemoveNode(index);
                }
            }
        }

    }
}