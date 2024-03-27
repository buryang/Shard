#pragma once
#include "Utils/Handle.h"
#include "Utils/CommonUtils.h"
#include "Utils/DirectedAcyclicGraph.h"
#include "Utils/FileArchive.h"
#include "Render/RenderPass.h"

namespace Shard
{
    namespace Render
    {
        static constexpr const uint32_t INVALID_INDEX = -1;
        static constexpr const uint32_t MAX_OUTPUT_NUM = 4u;

        class MINIT_API RenderGraph final : public Utils::DirectedAcyclicGraph
        {
        public:
            struct RenderOutputData
            {
                RenderPass::Handle  pass_;
                Field* field_{ nullptr };
            };
            struct RenderEdgeData
            {
                //two pass connected field count
                uint32_t    ref_count_;
            };
            struct RenderNodeData
            {
                RenderPass::Handle  pass_;
            };
            friend class RenderGraphBuilder;
            using BaseClass = Utils::DirectedAcyclicGraph;
        public:
            RenderGraph() = default;
            void AddPass(RenderPass::Handle pass);
            template <typename SetupLambda, typename ExecuteLambda, bool never_culled=false>
            void AddPass(const String& pass_name, EPipeline pipe, ExecuteLambda&& execute_lambda, SetupLambda&& setup_lambda=[](RenderPass::ScheduleContext&) {})
            {
                auto& pass_repo = RenderPass::PassRepoInstance();
                auto lambda_pass = pass_repo->Alloc<LambdaRenderPass<SetupLambda, ExecuteLambda>>(pass_name, pipe, never_culled, execute_lambda, setup_lambda);
                AddPass(lambda_pass);
            }
            void RemovePass(const RenderPass::Handle pass);
            //src field included in producer, dst field included in consumer
            bool ConnectPass(RenderPass::Handle producer, RenderPass::Handle consumer);
            void DisConnectPass(RenderPass::Handle producer, RenderPass::Handle consumer);
            bool ConnectField(RenderPass::Handle producer, Field& src, RenderPass::Handle consumer, Field& dst);
            void DisConnectField(RenderPass::Handle producer, Field& src, RenderPass::Handle consumer, Field& dst);
            //todo mark input
            void MarkOutput(RenderPass::Handle pass, Field& field);
            void UnMarkOutput(RenderPass::Handle pass, Field& field);
            bool ValidateGraph();
            void Sort();
            FORCE_INLINE uint32_t PassNum()const { return GetNodeCount(); }
            FORCE_INLINE uint32_t OutputNum()const { return outputs_.size(); }
            FORCE_INLINE RenderOutputData& GetOutput(uint32_t index) { return *std::next(outputs_.begin(), index); }
            FORCE_INLINE uint32_t GetPassIndex(const RenderPass::Handle pass)const {
                auto iter = pass_to_index_.find(pass);
                if (iter != pass_to_index_.end()) {
                    return iter->second;
                }
                return -1;
            }
            FORCE_INLINE RenderPass::Handle GetPass(uint32_t index)const { return node_data_[index].pass_; }
            void Serialize(Utils::IOArchive& ar);
        protected:
            bool IsPassMergeAble(RenderPass::Handle prev, RenderPass::Handle next)const;
        private:
            DISALLOW_COPY_AND_ASSIGN(RenderGraph);
        private:
            Map<RenderPass::Handle, uint32_t>   pass_to_index_;
            Map<uint32_t, RenderEdgeData> edge_data_;
            Map<uint32_t, RenderNodeData> node_data_;
            Set<RenderOutputData>  outputs_;
        };
    }
}
