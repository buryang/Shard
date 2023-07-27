#pragma once

#include "Renderer/RtRenderPass.h"
#include "Renderer/RtRenderGraphExe.h"
#include "RHI/RHIGlobalEntity.h"

namespace Shard
{
	namespace Renderer
	{
		class RtRendererGraph;
		class MINIT_API RtRenderGraphBuilder
		{ 
		public:
			struct BuildConfig
			{
				bool	aync_enable_{ false };
				bool	culling_passes_{ false };
				bool	res_aliasing_enable_{ false };
			};
			static RtRenderGraphExecutor::SharedPtr Compile(RtRendererGraph& graph, const BuildConfig& param);
		private:
			DISALLOW_COPY_AND_ASSIGN(RtRenderGraphBuilder);
			void CullingNoUsePasses(RtRendererGraph& graph);
			void AnalysisResourceUsage(const RtRendererGraph& graph, RtRenderGraphExecutor::SharedPtr executor);
			//build resource barrier
			void AddResourceTransition(RtRenderGraphExecutor::SharedPtr executor);
			bool ValidateFinalizeGraph(const RtRendererGraph& graph);
		};
	}
}