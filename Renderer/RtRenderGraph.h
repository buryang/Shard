#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/DirectedAcyclicGraph.h"
#include "Renderer/RtRenderPass.h"

namespace MetaInit
{
	namespace Renderer
	{
		class RtRendererPass;
		class RtField;
		class RtRenderResourceBridge;
		static constexpr const uint32_t INVALID_INDEX = -1;

		class MINIT_API RtRendererGraph : public Utils::DirectedAcyclicGraph<RtRendererPass, RtRenderResourceBridge>
		{
		public:
			using ParentType = Utils::DirectedAcyclicGraph<RtRendererPass, RtRenderResourceBridge>;
			using Node = ParentType::Node;
			using Edge = ParentType::Edge;
			virtual ~RtRendererGraph() = default;
			void AddPass(RtRendererPass::Ptr pass, const String& pass_name);
			void RemovePass(const String& pass_name);
			void Connect();
			void DisConnect();
			uint32_t PassNum()const { return pass_to_index_.size(); }
			uint32_t GetPassIndex(const String& pass_name)const;
			uint32_t OutputNum()const { return outputs_.size(); }
			//check whether graph is sorted
			bool IsSorted()const { return is_sorted_; }
			template <typename Function>
			void EnumerateOutputs() {
				for (auto output : outputs_) {
					Function(output);
				}
			}
		private:
			DISALLOW_COPY_AND_ASSIGN(RtRendererGraph);
		private:
			friend class RtRenderGraphBuilder;
			Map<String, uint32_t>		resource_to_index_;
			Map<String, uint32_t>		pass_to_index_;
			bool						is_recompile_needed_{ true };
			bool						is_sorted_{ false };
			SmallVector<RtField, 4>		outputs_;
			//external resource 

		};

		class RtRenderResourceBridge
		{
		public:
			using Ptr = RtRenderResourceBridge*;
			RtRenderResourceBridge(RtField& src, RtField& dst) :src_(src), dst_(dst) {
				dst_.ParentName(src_.GetName());
			}
			~RtRenderResourceBridge() {
				dst_.ParentName("");
			}
		private:
			RtField& src_;
			RtField& dst_;
		};

	}
}
