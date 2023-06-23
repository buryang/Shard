#pragma once
#include <optional>
#include "Utils/CommonUtils.h"

namespace Shard
{
	namespace Utils
	{
		class NodeData
		{
		public:
			//shuold not be so much node 
			enum class Flags : uint32_t
			{
				eValid			= 0x0,
				eGraphRoot		= 0x1,
				eGraphEnd		= 0x2,
				eInValid		= 0x100,
			};
			NodeData() = default;
			Flags GetFlags() const { return flags_; }
			void SetFlags(Flags flags) { flags_ = flags; }
			bool IsValid() const { return !Utils::HasAnyFlags(GetFlags(), Flags::eInValid); }
			void AddInEdge(uint32_t edge);
			void AddOutEdge(uint32_t edge);
			void RemoveInEdge(uint32_t edge_index);
			void RemoveOutEdge(uint32_t edge_index);
			FORCE_INLINE uint32_t GetIndex()const { return index_; }
			FORCE_INLINE uint32_t GetInEdge(uint32_t index)const { return in_edges_[index]; }
			FORCE_INLINE uint32_t GetOutEdge(uint32_t index)const { return out_edges_[index]; }
			FORCE_INLINE uint32_t GetInEdgeCount()const { return in_edges_.size(); }
			FORCE_INLINE uint32_t GetOutEdgeCount()const { return out_edges_.size(); }
		private:
			uint32_t				index_{ -1 };
			Flags					flags_{ Flags::eValid };
			SmallVector<uint32_t, 4>	in_edges_;
			SmallVector<uint32_t, 4>	out_edges_;
		};

		class EdgeData
		{
		public:
			EdgeData(uint32_t src, uint32_t dst, uint32_t index) :src_node_(src), dst_node_(dst), index_(index) {}
			uint32_t GetSrc() const { return src_node_; }
			uint32_t GetDst() const { return dst_node_; }
			FORCE_INLINE bool IsSrcValid() const { return src_node_ != -1; }
			FORCE_INLINE bool IsDstValid() const { return dst_node_ != -1; }
			FORCE_INLINE bool IsValid() const { return IsSrcValid() && IsDstValid() && src_node_ != dst_node_; }
			FORCE_INLINE uint32_t GetIndex() const { return index_; }
		private:
			uint32_t		index_{ -1 };
			uint32_t		src_node_{ -1 };
			uint32_t		dst_node_{ -1 };
		};

		class DirectedAcyclicGraph
		{
		public:
			using Node = NodeData;
			using Edge = EdgeData;
			using CallBack = std::function<void(uint32_t index)>;
			DirectedAcyclicGraph() = default;
			virtual ~DirectedAcyclicGraph() {}
			void AddEdge(uint32_t src_node, uint32_t dst_node, uint32_t edge_index);
			void RemoveEdge(uint32_t index);
			void AddNode(uint32_t node_index);
			void RemoveNode(uint32_t node_index);
			//check whether this's valid acyclic graph
			bool IsValid() const;
			//check whether two node is connect
			bool IsConnected(uint32_t lhs_node, uint32_t rhs_node) const;
			//delete no used node and edge
			void Truncate();
			Node* GetNode(uint32_t node_index);
			Edge* GetEdge(uint32_t edge_index);
		private:
			template<typename Method>
			friend class DirectGraphVisitor;
			DISALLOW_COPY_AND_ASSIGN(DirectedAcyclicGraph);
		protected://ugly code 
			CallBack add_edge_clk_{ [](auto) {} };
			CallBack add_node_clk_{ [](auto) {} };
			CallBack rm_edge_clk_{ [](auto) {} };
			CallBack rm_node_clk_{ [](auto) {} };
		private:
			Map<uint32_t, Node>	nodes_;
			Map<uint32_t, Edge>	edges_;
			SmallVector<uint32_t>	root_;
			SmallVector<uint32_t>	end_;
		};

		template<typename Node>
		struct BFSSearch
		{
			using Container = Stack<Node>;
			static void Push(Container& container, Node& node) {
				container.push(node);
			}
			static Node Pop(Container& container) {
				auto node = container.top();
				container.pop();
				return node;
			}
		};

		template<typename Node>
		struct DFSSearch
		{
			using Container = Queue<Node>;
			static void Push(Container& container, Node& node) {
				container.push(node);
			}
			static Node Pop(Container& container) {
				auto node = container.front();
				container.pop();
				return node;
			}
		};

		template<typename Method>
		class DirectGraphVisitor
		{
		public:
			using Graph = DirectedAcyclicGraph;
			using Node = Graph::Node;
			using Edge = Graph::Edge;
			DirectGraphVisitor(const Graph& graph, const Node* begin) :graph_(graph), from_(begin) {}
			Node* Next() {
				//context stack logic
				if (vis_context_.empty())
				{
					return nullptr;
				}

				auto node = Method::Pop(vis_context_);
				for (auto n = 0; n < node->GetOutEdgeCount(); ++n) {
					auto edge_index = node->GetOutEdge(n);
					auto edge = graph_.GetEdge(edege_index);
					if (nullptr != edge && edge->IsValid()) {
						auto next_node = graph_.GetNode(edge->GetDst());
						Method.Push(vis_context_, next_node);
					}
				}
				return node;
			}
		private:
			const Node* from_;
			Method<Node>::Container vis_context_;
			const DirectedAcyclicGraph& graph_;
		};

	}
}

#include "Utils/DirectedAcyclicGraph.inl"