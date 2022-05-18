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
				edges_[curr_edge_index] = { src_node, dst_node };
				src_node.AddOutEdge(curr_edge_index);
				dst_node.AddInEdge(curr_edge_index);
				++curr_edge_index;
			}
		}

		template<typename NodeHandle, typename EdgeHandle>
		inline void DirectedAcyclicGraph<NodeHandle, EdgeHandle>::RemoveEdge(const uint32_t index)
		{
			auto edge = GetEdge(index);
			if (edge != std::nullopt)
			{
				GetNode(edge.GetSrc()).RemoveOutEdge(index);
				GetNode(edge.GetDst()).RemoveInEdge(index);
				edges_.erase(index);
			}
		}

		template<typename NodeHandle, typename EdgeHandle>
		inline void DirectedAcyclicGraph<NodeHandle, EdgeHandle>::AddNode(NodeHandle handle)
		{
			nodes_[curr_node_index_++] = Node(handle);
		}

		template<typename NodeHandle, typename EdgeHandle>
		inline void DirectedAcyclicGraph<NodeHandle, EdgeHandle>::RemoveNode(uint32_t node_index)
		{
			auto node = GetNode(node_index);
			if (std::nullopt != node)
			{
				for (auto n = 0; n < node.GetInEdgeCount(); ++n)
				{
					node.RemoveInEdge();
				}

				for (auto n = 0; n < node.GetOutEdgeCount(); ++n)
				{
					node.RemoveOutEdge();
				}
			}
		}

		template<typename NodeHandle, typename EdgeHandle>
		inline std::optional<DirectedAcyclicGraph<NodeHandle, EdgeHandle>::Node> DirectedAcyclicGraph<NodeHandle, EdgeHandle>::GetNode(uint32_t node_index)
		{
			if (nodes_.find(node_index) != nodes_.end())
			{
				return nodes_[node_index];
			}
			return std::nullopt;
		}

		template<typename NodeHandle, typename EdgeHandle>
		inline std::optional<DirectedAcyclicGraph<NodeHandle, EdgeHandle>::Edge> DirectedAcyclicGraph<NodeHandle, EdgeHandle>::GetEdge(uint32_t edge_index)
		{
			if (edges_.find(edge_index) != edges_.end())
			{
				return edges_[edge_index];
			}
			return std::nullopt;
		}
	}
}