#include <algorithm>
#include "DirectedAcyclicGraph.h"

namespace MetaInit
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

		void DirectedAcyclicGraph::AddEdge(uint32_t src_node_index, uint32_t dst_node_index, uint32_t edge_index)
		{
			auto src_node = GetNode(src_node_index);
			auto dst_node = GetNode(dst_node_index);

			if (src_node && dst_node)
			{
				edges_[edge_index] = {src_node_index, dst_node_index, edge_index};
				src_node->AddOutEdge(edge_index);
				dst_node->AddInEdge(edge_index);
				add_edge_clk_(edge_index);
			}
		}

		void DirectedAcyclicGraph::RemoveEdge(uint32_t index)
		{
			auto edge = GetEdge(index);
			if (edge != nullptr)
			{
				GetNode(edge->GetSrc())->RemoveOutEdge(index);
				GetNode(edge->GetDst())->RemoveInEdge(index);
				edges_.erase(index);
				rm_edge_clk_(index);
			}
		}

		void DirectedAcyclicGraph::AddNode(uint32_t node_index)
		{
			nodes_.insert(eastl::make_pair(node_index, Node()));
			add_node_clk_(node_index);
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
				rm_node_clk_(node_index);
			}
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

		bool DirectedAcyclicGraph::IsValid() const
		{
			//check whether graph is acylic, ugly code now
			for (auto& node : nodes_)
			{
				if (IsConnected(node.first, node.first)) {
					return false;
				}
			}
			return true;
		}

		bool DirectedAcyclicGraph::IsConnected(uint32_t lhs_node, uint32_t rhs_node) const
		{
			DirectGraphVisitor<BFSSearch<DirectedAcyclicGraph::Node>> visitor(*this, const_cast<DirectedAcyclicGraph*>(this)->GetNode(lhs_node));
			do{
				auto next = visitor.Next();
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
				if (!node.IsValid()) {
					RemoveNode(index);
				}
			}
		}

	}
}