#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/DirectedAcyclicGraph.h"
#include "RHI/VulkanPrimitive.h"
#include <string>
#include <unordered_map>
#include <memory>

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
			using Node = Utils::DirectedAcyclicGraph<RtRendererPass, RtRenderResourceBridge>::Node;
			using Edge = Utils::DirectedAcyclicGraph<RtRendererPass, RtRenderResourceBridge>::Edge;
			virtual ~RtRendererGraph() = default;
			void AddPass(RtRendererPass* pass, const std::string& pass_name);
			void RemovePass(const std::string& pass_name);
			//void addfield(rtrenderresource<>::ptr texture, const std::string& field_name);
			//void addfield(rtrenderresource<>::ptr buffer, const std::string& field_name);
			uint32_t PassNum()const { return pass_to_index_.size(); }
			uint32_t GetPassIndex(const std::string& pass_name);
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
			std::unordered_map<std::string, uint32_t>	resource_to_index_;
			std::unordered_map<std::string, uint32_t>	pass_to_index_;
			bool										is_recompile_needed_{ true };
			bool										is_sorted_{ false };
			SmallVector<RtField>						outputs_;
			//external resource 

		};

		/*
		* try to share pin resource between diff node
		* 1-to-N N-to-1 1-to-1 N-to-N adapter
		*/
		class RtRenderResourceBridge
		{
		public:
			using PinType = RtField*;
			void AddSrcPin(PinType pin);
			void AddDstPin(PinType pin);
			void RemoveSrcPin(PinType pin);
			void RemoveDstPin(PinType pin);
			void OnCreateEdge(const RtRendererGraph::Edge* edge);
			void OnRemoveEdge(const RtRendererGraph::Edge* edge);
			bool IsOptimizeNeeded()const;
			void Optimize();
		private:
			class ResourceBridgeMergeHelper
			{
			public:
				ResourceBridgeMergeHelper(RtRenderResourceBridge& bridge) : bridge_(bridge) {}
				bool operator()(void);
			private:
				RtRenderResourceBridge& bridge_;
			};

			class ResourceBridgeTransiantHelper
			{
			public:
				ResourceBridgeTransiantHelper(RtRenderResourceBridge& bridge) : bridge_(bridge) {}
				bool operator()(void);
			private:
				RtRenderResourceBridge& bridge_;
			};

			class ResourceBridgeTransitionHelper
			{
			public:
				ResourceBridgeTransitionHelper(RtRenderResourceBridge& bridge) : bridage_(bridge) {}
				bool operator()(void);
			private:
				RtRenderResourceBridge& bridge_;
			};
			
			//decide which pins can be merged
			void OptimizeMergeAblePins();
			//decide which pins can be transiant
			void OptimizeTransiantAblePins();
		private:
			SmallVector<PinType>	src_field_;
			SmallVector<PinType>	dst_field_;
			union {
				RtRenderResource<RHITexture>		rhi_buffer_;
				RtRenderResource<RHIBuffer>			rhi_texture_;
				RtRenderResource<RHIAccelerate>		rhi_accelerate_;
			};
		};
	}
}
