#include <algorithm>
#include "DirectedAcyclicGraph.h"

namespace MetaInit
{
	namespace Utils
	{
		template<typename NodeHandle>
		inline void NodeData<NodeHandle>::AddInEdge(uint32_t edge)
		{
			if (in_edges_.find(edge) == in_edges_.end() &&
				out_edges_.find(edge) == out_edges_.end())
			{
				in_edges_.push_back(edge);
			}
		}

		template<typename NodeHandle>
		inline void NodeData<NodeHandle>::AddOutEdge(uint32_t edge)
		{
			if (in_edges_.find(edge) == in_edges_.end() &&
				out_edges_.find(edge) == out_edges_.end())
			{
				out_edges_.push_back(edge);
			}
		}

		template<typename NodeHandle>
		inline void NodeData<NodeHandle>::RemoveInEdge(uint32_t edge)
		{
			auto iter = in_edges_.find(edge);
			if (iter != in_edges_.end())
			{
				std::swap(*iter, in_edges_.back());
				in_edges_.pop_back();
			}
		}

		template<typename NodeHandle>
		inline void NodeData<NodeHandle>::RemoveOutEdge(uint32_t edge)
		{
			auto iter = out_edges_.find(edge);
			if (iter != out_edges_.end())
			{
				std::swap(*iter, out_edges_.back());
				out_edges_.pop_back();
			}
		}

		template<typename EdgeHandle>
		inline uint32_t EdgeData<EdgeHandle>::GetSrc() const
		{
			return src_node_;
		}

		template<typename EdgeHandle>
		inline uint32_t EdgeData<EdgeHandle>::GetDst() const
		{
			return dst_node_;
		}

		template<typename NodeHandle, typename EdgeHandle>
		inline void DirectedAcyclicGraph<NodeHandle, EdgeHandle>::AddEdge(uint32_t src_node, uint32_t dst_node)
		{
			auto src_node = GetNode(src_node);
			auto dst_node = GetNode(dst_node);

			if (src_node && dst_node)
			{
				auto ptr = allocator.Alloc(sizeof(Edge));
				new (ptr)Edge({ src_node, dst_node });
				edges_[curr_edge_index] = reinterpret_cast<Edge*>(ptr);
				src_node.AddOutEdge(curr_edge_index);
				dst_node.AddInEdge(curr_edge_index);
				++curr_edge_index;
			}
		}

		template<typename NodeHandle, typename EdgeHandle>
		inline void DirectedAcyclicGraph<NodeHandle, EdgeHandle>::RemoveEdge(const uint32_t index)
		{
			auto edge = GetEdge(index);
			if (edge != nullptr)
			{
				GetNode(edge.GetSrc()).RemoveOutEdge(index);
				GetNode(edge.GetDst()).RemoveInEdge(index);
				edges_.erase(index);
			}
		}

		template<typename NodeHandle, typename EdgeHandle>
		inline void DirectedAcyclicGraph<NodeHandle, EdgeHandle>::AddNode(NodeHandle&& node)
		{
			auto ptr = allocator_.Alloc(sizeof(Node));
			new (ptr) Node(node);
			nodes_.insert(std::make_pair(curr_node_index_++, reinterpret_cast<Node*>(ptr)));
		}

		template<typename NodeHandle, typename EdgeHandle>
		inline void DirectedAcyclicGraph<NodeHandle, EdgeHandle>::RemoveNode(uint32_t node_index)
		{
			auto node = GetNode(node_index);
			if (nullptr != node)
			{
				for (auto n = 0; n < node.GetInEdgeCount(); ++n)
				{
					node.RemoveInEdge();
				}

				for (auto n = 0; n < node.GetOutEdgeCount(); ++n)
				{
					node.RemoveOutEdge();
				}
				nodes_.erase(node_index);
			}
		}

		template<typename NodeHandle, typename EdgeHandle>
		inline DirectedAcyclicGraph<NodeHandle, EdgeHandle>::Node* DirectedAcyclicGraph<NodeHandle, EdgeHandle>::GetNode(uint32_t node_index)
		{
			if (nodes_.find(node_index) != nodes_.end())
			{
				return nodes_[node_index];
			}
			return nullptr;
		}

		template<typename NodeHandle, typename EdgeHandle>
		inline DirectedAcyclicGraph<NodeHandle, EdgeHandle>::Edge* DirectedAcyclicGraph<NodeHandle, EdgeHandle>::GetEdge(uint32_t edge_index)
		{
			if (edges_.find(edge_index) != edges_.end())
			{
				return edges_[edge_index];
			}
			return nullptr;
		}

		template<typename NodeHandle, typename EdgeHandle>
		inline bool DirectedAcyclicGraph<NodeHandle, EdgeHandle>::IsValid() const
		{
			//check whether graph is acylic, ugly code now
			for (auto node : nodes_)
			{
				if (IsConnected(node->second, node->second)) {
					return false;
				}
			}
			return true;
		}

		template<typename NodeHandle, typename EdgeHandle>
		inline bool DirectedAcyclicGraph<NodeHandle, EdgeHandle>::IsConnected(const DirectedAcyclicGraph<NodeHandle, EdgeHandle>::Node* lhs, 
																				const DirectedAcyclicGraph<NodeHandle, EdgeHandle>::Node* rhs) const
		{
			DirectGraphVisitor<BFSSearch<DirectedAcyclicGraph<NodeHandle, EdgeHandle>::Node>, NodeHandle, EdgeHandle> visitor(*this, lhs);

			do{
				auto next = visitor.Next();
				if (nullptr == next) {
					break;
				}
				//check pointer 
				if (next == rhs) {
					return true;
				}

			} while (true);
			return false;
		}

		template<typename NodeHandle, typename EdgeHandle>
		inline void DirectedAcyclicGraph<NodeHandle, EdgeHandle>::Truncate()
		{
			for(auto )
		}

		template<template<typename> typename Method, class Graph>
		inline Node* DirectGraphVisitor<Method, Graph>::Next()
		{
			//context stack logic
			if (container_.empty())
			{
				return nullptr;
			}

			auto node = Method::Pop(vis_context_);
			for (auto n = 0; n < node->GetOutEdgeCount(); ++n) {
				auto edge_index = node->GetOutEdge(n);
				auto edge = graph_.GetEdge(edege_index);
				if (nullptr != edge && edge->IsValid()) {
					auto next_node = graph_.GetNode(edge->GetDst());
					Method.Push(next_node);
				}
			}
			return node;
		}
	}
}