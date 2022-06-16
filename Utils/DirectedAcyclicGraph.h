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
			//shuold not be so mush node 
			enum class Flags : uint32_t
			{
				eValid			= 0x0,
				eInValid		= 0x1,
				eGraphRoot		= 0x2,
				eGraphEnd		= 0x4,
			};
			NodeData() = default();
			NodeData(Handle handle) :handle_(handle) {}
			Handle Get() { return handle_; }
			Flags GetFlags() const { return flags_; }
			void SetFlags(Flags flags) { flags_ = flags; }
			bool IsValid() const { return !(GetFlags() & Flags::eInValid); }
			void AddInEdge(uint32_t edge);
			void AddOutEdge(uint32_t edge);
			void RemoveInEdge(uint32_t edge_index);
			void RemoveOutEdge(uint32_t edge_index);
			uint32_t GetIndex()const { return index_; }
			uint32_t GetInEdge(uint32_t index)const { return in_edges_[index]; }
			uint32_t GetOutEdge(uint32_t index)const { return out_edges_[index]; }
			uint32_t GetInEdgeCount()const { return in_edges_.size(); }
			uint32_t GetOutEdgeCount()const { return out_edges_.size(); }
		private:
			uint32_t				index_{ -1 };
			Handle					handle_;
			Flags					flags_{ Flags::eValid };
			SmallVector<uint32_t>	in_edges_;
			SmallVector<uint32_t>	out_edges_;
		};

		template<typename EdgeHandle>
		class EdgeData
		{
		public:
			using Handle = EdgeHandle;
			EdgeData(uint32_t src, uint32_t dst) :src_node_(src), dst_node_(dst) {}
			Handle Get() { return handle_; }
			uint32_t GetSrc()const;
			uint32_t GetDst()const;
			bool IsSrcValid()const { return src_node_ != -1; }
			bool IsDstValid()const { return dst_node_ != -1; }
			bool IsValid()const { return IsSrcValid() && IsDstValid(); }
		private:
			Handle			handle_;
			uint32_t		src_node_{ -1 };
			uint32_t		dst_node_{ -1 };
		};

		template<typename NodeHandle, typename EdgeHandle>
		class DirectedAcyclicGraph
		{
		public:
			using Node = NodeData<NodeHandle>;
			using Edge = EdgeData<EdgeHandle>;
			DirectedAcyclicGraph(Allocator* alloc) :allocator_(alloc) {}
			void AddEdge(uint32_t src_node, uint32_t dst_node);
			void RemoveEdge(const uint32_t index);
			void AddNode(NodeHandle&& node);
			void RemoveNode(uint32_t node_index);
			Node* GetNode(uint32_t node_index);
			Edge* GetEdge(uint32_t edge_index);
			//check whether this's valid acyclic graph
			bool IsValid() const;
			//check whether two node is connect
			bool IsConnected(const Node* lhs, const Node* rhs) const;
			//delete no used node and edge
			void Truncate();
		private:
			template <typename, typename, typename>
			friend class DirectGraphVisitor;
			DISALLOW_COPY_AND_ASSIGN(DirectedAcyclicGraph);
		private:
			Allocator*					allocator_{ nullptr };
			std::map<uint32_t, Node*>	nodes_;
			std::map<uint32_t, Edge*>	edges_;
			SmallVector<uint32_t>		root_;
			SmallVector<uint32_t>		end_;
			uint32_t					curr_node_index_{ 0 };
			uint32_t					curr_edge_index_{ 0 };
		};

		template<typename Node>
		struct BFSSearch
		{
			using Container = std::stack<Node>;
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
			using Container = std::queue<Node>;
			static void Push(Container& container, Node& node) {
				container.push(node);
			}
			static Node Pop(Container& container) {
				auto node = container.front();
				container.pop();
				return node;
			}
		};

		template<template<typename> typename Method, class Graph>
		class DirectGraphVisitor
		{
		public:
			using Graph::Node;
			using Graph::Edge;
			DirectGraphVisitor(const Graph& graph, const Node* begin) :graph_(graph), from_(begin) {}
			Node* Next();
		private:
			const Node* from_;
			Method<Node>::Container vis_context_;
			const DirectedAcyclicGraph<NodeHandle, EdgeHandle>& graph_;
		};

	}
}

#include "Utils/DirectedAcyclicGraph.inl"