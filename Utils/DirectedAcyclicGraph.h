#pragma once
#include "Utils/CommonUtils.h"

namespace Shard
{
    namespace Utils
    {
        constexpr uint32_t INVALID_INDEX = -1u;

        class Node
        {
        public:
            //shuold not be so much node 
            explicit Node(uint32_t index = INVALID_INDEX) :index_(index) {}
            void Evict(bool evict) { alive_ = !evict; }
            bool IsEvicted() const { return !!alive_; }
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
            uint32_t    alive_ : 1{ 0u };
            uint32_t    index_ : 31{ INVALID_INDEX }; //maxnum of node
            SmallVector<uint32_t, 4>    in_edges_;
            SmallVector<uint32_t, 4>    out_edges_;
        };

        class Edge
        {
        public:
            Edge(uint32_t src, uint32_t dst, uint32_t index) :src_node_(src), dst_node_(dst), index_(index) {}
            uint32_t GetSrc() const { return src_node_; }
            uint32_t GetDst() const { return dst_node_; }
            FORCE_INLINE bool IsSrcValid() const { return src_node_ != INVALID_INDEX; }
            FORCE_INLINE bool IsDstValid() const { return dst_node_ != INVALID_INDEX; }
            FORCE_INLINE bool IsValid() const { return IsSrcValid() && IsDstValid() && src_node_ != dst_node_; }
            FORCE_INLINE uint32_t GetIndex() const { return index_; }
        private:
            uint32_t    index_{ INVALID_INDEX };
            uint32_t    src_node_{ INVALID_INDEX };
            uint32_t    dst_node_{ INVALID_INDEX };
        };

        class DirectedAcyclicGraph
        {
        public:
            DirectedAcyclicGraph() = default;
            virtual ~DirectedAcyclicGraph() = default;
            uint32_t AddEdge(uint32_t src_node, uint32_t dst_node);
            uint32_t GetEdgeIndex(uint32_t src_node, uint32_t dst_node);
            void RemoveEdge(uint32_t index);
            uint32_t AddNode();
            void RemoveNode(uint32_t node_index);
            uint32_t GetNodeCount()const { return nodes_.size(); }
            uint32_t GetEdgeCount()const { return edges_.size(); }
            //check whether this's valid acyclic graph
            bool TestCyclicDetection() const;
            //check whether two node is connect
            bool IsConnected(uint32_t lhs_node, uint32_t rhs_node) const;
            //delete no used node and edge
            void Truncate();
            /**
             * do graph topology sort
             */
            void TopoSort();
            Node* GetNode(uint32_t node_index);
            Edge* GetEdge(uint32_t edge_index);
            virtual ~DirectedAcyclicGraph() = default;
        protected:
            /**
             * calc longest path distance
             * \param src: src node
             * \param dst: dst node
             * \return longest path distance 
             */
            float LongestPath(uint32_t src_node, uint32_t dst_node)const;
            /**
             * \param src
             * \param dst
             * \return 
             */
            float ShortestPath(uint32_t src_nodex, uint32_t dst_node)const;
            /**
             * test whether orphan edge exist
             */
            bool TestEdgeOrphan()const; 
            Vector<uint32_t>& GetOrderedNodes() { return ordered_nodes_; }
        private:
            template<class, template<typename> class>
            friend class DirectGraphVisitor;
            DISALLOW_COPY_AND_ASSIGN(DirectedAcyclicGraph);
        protected:
            Map<uint32_t, Node>    nodes_;
            Map<uint32_t, Edge>    edges_;
            Vector<uint32_t>    ordered_nodes_;         
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

        //refactor to graph iterator
        template<class Graph, template<typename> class Method>
        class DirectGraphVisitor
        {
        public:
            using Node = typename Graph::Node;
            using Edge = typename Graph::Edge;
            using Container = typename Method<Node>::Container;
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
                        Method::Push(vis_context_, next_node);
                    }
                }
                return node;
            }
        private:
            const Node* from_{ nullptr };
            Container vis_context_;
            const Graph& graph_;
        };

    }
}

