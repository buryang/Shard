#pragma once
#include <optional>
#include "Utils/CommonUtils.h"

namespace Shard
{
    namespace Utils
    {
        constexpr uint32_t INVALID_INDEX = -1u;

        class NodeData
        {
        public:
            //shuold not be so much node 
            enum class EFlags : uint32_t
            {
                eValid            = 0x0,
                eGraphRoot        = 0x1,
                eGraphEnd        = 0x2,
                eInValid        = 0x100,
            };
            NodeData() = default;
            EFlags GetFlags() const { return flags_; }
            void SetFlags(EFlags flags) { flags_ = flags; }
            bool IsValid() const { return !Utils::HasAnyFlags(GetFlags(), EFlags::eInValid); }
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
            uint32_t                index_{ INVALID_INDEX };
            EFlags                    flags_{ EFlags::eValid };
            SmallVector<uint32_t, 4>    in_edges_;
            SmallVector<uint32_t, 4>    out_edges_;
        };

        class EdgeData
        {
        public:
            EdgeData(uint32_t src, uint32_t dst, uint32_t index) :src_node_(src), dst_node_(dst), index_(index) {}
            uint32_t GetSrc() const { return src_node_; }
            uint32_t GetDst() const { return dst_node_; }
            FORCE_INLINE bool IsSrcValid() const { return src_node_ != INVALID_INDEX; }
            FORCE_INLINE bool IsDstValid() const { return dst_node_ != INVALID_INDEX; }
            FORCE_INLINE bool IsValid() const { return IsSrcValid() && IsDstValid() && src_node_ != dst_node_; }
            FORCE_INLINE uint32_t GetIndex() const { return index_; }
        private:
            uint32_t        index_{ INVALID_INDEX };
            uint32_t        src_node_{ INVALID_INDEX };
            uint32_t        dst_node_{ INVALID_INDEX };
        };

        class DirectedAcyclicGraph
        {
        public:
            using Node = NodeData;
            using Edge = EdgeData;
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
            /**
             * do graph topology sort
             */
            void Sort();
            Node* GetNode(uint32_t node_index);
            Edge* GetEdge(uint32_t edge_index);
        private:
            /**
             * calc longest path distance
             * \param src: src node
             * \param dst: dst node
             * \return longest path distance 
             */
            float LongestPath(const Node* src, const Node* dst);
            /**
             * \param src
             * \param dst
             * \return 
             */
            float ShortestPath(const Node* src, const Node* dst);
            /**
             * test whether orphan edge exist
             */
            bool TestEdgeOrphan()const; 
        private:
            template<class, template<typename> class>
            friend class DirectGraphVisitor;
            DISALLOW_COPY_AND_ASSIGN(DirectedAcyclicGraph);
        protected://ugly code 
            virtual void PostAddEdge(uint32_t edge_index) {}
            virtual void PostAddNode(uint32_t node_index) {}
            virtual void PostRemoveEdge(uint32_t edge_index) {}
            virtual void PostRemoveNode(uint32_t node_index) {}
        private:
            Map<uint32_t, Node>    nodes_;
            Map<uint32_t, Edge>    edges_;
            SmallVector<uint32_t>    root_;
            SmallVector<uint32_t>    end_;
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
            using Node = Graph::Node;
            using Edge = Graph::Edge;
            using Container = Method<Node>::Container;
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
            const Node* from_{ nullptr };
            Container vis_context_;
            const Graph& graph_;
        };

    }
}

