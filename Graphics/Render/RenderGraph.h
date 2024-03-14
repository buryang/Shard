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
            RenderGraph() = default;
            void AddPass(RenderPass::Handle pass);
            template<typename LAMBDA, typename ParametersStruct>
            void AddPass(LAMBDA&& lambda, EPipeline pipe, RenderPass::EFlags flags, const String& pass_name, ParametersStruct& parameters)
            {
                auto& pass_repo = RenderPass::PassRepoInstance();
                auto lambda_pass = pass_repo->Alloc<LambdaRenderPass<LAMBDA>>(pass_name, pipe, flags, lambda);
                for (auto f : parameters.) {
                    //add pass parameters here
                    lambda_pass->GetScheduleContext().AddField(f);
                }
                AddPass(lambda_pass, pass_name);
            }
            void RemovePass(const RenderPass::Handle pass);
            void InsertPass(RenderPass::Handle prev_pass, RenderPass::Handle pass);
            //src field included in producer, dst field included in consumer
            void ConnectPass(RenderPass::Handle producer, RenderPass::Handle consumer, Field& src, Field& dst);
            void DisConnectField(Field& src, Field& dst);
            FORCE_INLINE uint32_t PassNum()const { return pass_to_index_.size(); }
            FORCE_INLINE uint32_t GetPassIndex(const RenderPass::Handle pass)const;
            FORCE_INLINE uint32_t GetEdgeIndex(const String& edge_name)const;
            FORCE_INLINE uint32_t OutputNum()const { return outputs_.size(); }
            //check whether graph is sorted
            FORCE_INLINE bool IsSorted()const { return is_sorted_; }
            FORCE_INLINE bool IsCompileNeeded()const { return is_recompile_needed_; }
            FORCE_INLINE bool IsBuildWithConf(const uint32_t build_conf) const { return build_flags_ == build_conf; }
            FORCE_INLINE void SetBuildConf(const uint32_t build_conf) { build_flags_ = build_conf; }
            template <typename Function>
            void EnumerateOutputs() {
                for (auto output : outputs_) {
                    Function(output);
                }
            }
            void Serialize(Utils::IOArchive& ar);
        protected:
            uint32_t CalcPassDependencyLevel(RenderPass::Handle pass); 
            bool IsPassMergeable(RenderPass::Handle prev, RenderPass::Handle next);
        private:
            DISALLOW_COPY_AND_ASSIGN(RenderGraph);
        private:
            friend class RenderGraphBuilder;
            uint32_t    is_recompile_needed_ : 1{0u};
            uint32_t    is_sorted_ : 1{0u};
            uint32_t    build_flags_ : 24{0u};
            Map<RenderPass::Handle, uint32_t>   pass_to_index_;
            Map<Field::Name, uint32_t>  resource_to_index_;
            SmallVector<Field, MAX_OUTPUT_NUM>  outputs_;
            Vector<uint8_t> pass_depend_level_;

            //ordered pass as executing order
            Vector<RenderPass::Handle> ordered_pass_;
        };
    }
}
