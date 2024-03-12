#pragma once
#include "Utils/Handle.h"
#include "Utils/CommonUtils.h"
#include "Utils/DirectedAcyclicGraph.h"
#include "Utils/FileArchive.h"
#include "Renderer/RtRenderPass.h"

namespace Shard
{
    namespace Renderer
    {
        class RtRendererPass;
        class RtField;
        class RtRenderFieldBridge;
        static constexpr const uint32_t INVALID_INDEX = -1;

        class MINIT_API RtRendererGraph final : public Utils::DirectedAcyclicGraph
        {
        public:
            RtRendererGraph();
            void AddPass(RtRendererPass::Ptr pass, const String& pass_name);
            template<typename LAMBDA, typename ParametersStruct>
            void AddPass(LAMBDA&& lambda, EPipeLine pipe, RtRendererPass::EFlags flags, const String& pass_name, ParametersStruct& parameters)
            {
                RtLambdaRendererPass<LAMBDA> lambda_pass(pass_name, pipe, flags, lambda);
                for (auto f : parameters.) {
                    //add pass parameters here
                    lambda_pass.GetSchduleContext().AddField(f);
                }
                AddPass(&lambda_pass, pass_name);
            }
            void RemovePass(const String& pass_name);
            //src field included in producer, dst field included in consumer
            void ConnectPass(PassHandle producer, PassHandle consumer, RtField& src, RtField& dst);
            void DisConnectPass(RtField& src, RtField& dst);
            FORCE_INLINE uint32_t PassNum()const { return pass_to_index_.size(); }
            FORCE_INLINE uint32_t GetPassIndex(const String& pass_name)const;
            FORCE_INLINE uint32_t GetEdgeIndex(const String& edge_name)const;
            FORCE_INLINE uint32_t OutputNum()const { return outputs_.size(); }
            //check whether graph is sorted
            FORCE_INLINE bool IsSorted()const { return is_sorted_; }
            FORCE_INLINE bool IsCompileNeeded()const { return is_recompile_needed_; }
            template <typename Function>
            void EnumerateOutputs() {
                for (auto output : outputs_) {
                    Function(output);
                }
            }

            void Serialize(Utils::IOArchive& ar);
            void UnSerialize(Utils::IOArchive& ar);
            //resource delegate todo
        protected:
            uint32_t CalcPassDependencyLevel(PassHandle pass); //todo
        private:
            friend class RtRenderGraphBuilder;
            DISALLOW_COPY_AND_ASSIGN(RtRendererGraph);
            FORCE_INLINE const String& GetEdgeNameFromFieldPair(const String& src_field, const String& dst_field)const {
                return src_field + "_" + dst_field;
            }
        private:
            friend class RtRenderGraphBuilder;
            Map<String, uint32_t>    edge_to_index_;
            Map<String, uint32_t>    pass_to_index_;
            bool    is_recompile_needed_{ true };
            bool    is_sorted_{ false };
            SmallVector<RtField&, 4>    outputs_;
            //external resource 
            
            /*manage every resource field liftetime*/
            class RtRenderFieldBridge
            {
            public:
                using Ptr = RtRenderFieldBridge*;
                RtRenderFieldBridge(RtField& src, RtField& dst) :src_(src), dst_(dst) {
                    if (!src_.IsConnectAble(dst_)) {
                        //cannot connect two field
                    }
                    dst_.ParentName(src_.GetName());
                    src_.InCrRef(); dst_.InCrRef();
                }
                ~RtRenderFieldBridge() {
                    dst_.ParentName("");
                    src_.DecrRef(); dst_.DecrRef();
                }
            private:
                DISALLOW_COPY_AND_ASSIGN(RtRenderFieldBridge);
                RtField& src_;
                RtField& dst_;
            };
            //pass and edge resource manager
            Utils::ResourceManager<Utils::Allocator, RtRendererPass>    pass_manager_;
            Utils::ResourceManager<Utils::Allocator, RtRenderFieldBridge>    edge_manager_;

        };
    }
}
