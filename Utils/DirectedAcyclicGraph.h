#pragma once
#include <optional>
#include "Utils/CommonUtils.h"

namespace MetaInit
{
	namespace Utils
	{

		template<typename NodeHandle>
		class NodeData
		{
		public:
			using Handle = NodeHandle;
			NodeData() = default();
			NodeData(Handle handle) :handle_(handle) {}
			Handle Get() { return handle_; }
			void AddInEdge(uint32_t edge);
			void AddOutEdge(uint32_t edge);
			void RemoveInEdge(uint32_t edge_index);
			void RemoveOutEdge(uint32_t edge_index);
			uint32_t GetInEdgeCount()const { return in_edges_.size(); }
			uint32_t GetOutEdgeCount()const { return out_edges_.size(); }
		private:
			uint32_t				index{ -1 };
			Handle					handle_;
			SmallVector<uint32_t>	in_edges_;
			SmallVector<uint32_t>	out_edges_;

		};

		template<typename EdgeHandle>
		class EdgeData
		{
		public:
			using Handle = EdgeHandle;
			EdgeData(uint32_t src, uint32_t dst) :src_node_(src), dst_node_(dst) {}
			uint32_t GetSrc()const;
			uint32_t GetDst()const;
		private:
			uint32_t		src_node_;
			uint32_t		dst_node_;
		};

		template<typename NodeHandle, typename EdgeHandle>
		class DirectedAcyclicGraph
		{
		public:
			using Node = NodeData<NodeHandle>;
			using Edge = EdgeData<EdgeHandle>;
			void AddEdge(uint32_t src_node, uint32_t dst_node);
			void RemoveEdge(const uint32_t index);
			void AddNode(NodeHandle node);
			void RemoveNode(uint32_t node_index);
			std::optional<Node> GetNode(uint32_t node_index);
			std::optional<Edge>	GetEdge(uint32_t edge_index);
		private:
			std::map<uint32_t, Node>	nodes_;
			std::map<uint32_t, Edge>	edges_;
			uint32_t		curr_node_index_{ 0 };
			uint32_t		curr_edge_index_{ 0 };
		};

	}
}

#include "Utils/DirectedAcyclicGraph.inl"