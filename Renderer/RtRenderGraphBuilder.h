#pragma once

#include "Renderer/RtRenderPass.h"
#include "Renderer/RtRenderGraphExe.h"
#include "RHI/RHI.h"

namespace MetaInit
{
	namespace Renderer
	{
		class RtRendererGraph;
		class RtRenderGraphBuilder
		{
		public:
			typedef struct _BuildParams
			{
				bool	aync_enable_{ false };
				bool	culling_passes_{ false };
				bool	res_aliasing_enable_{ false };
			}Params;
			RtRenderGraphExecutor::Ptr Build(RtRendererGraph::Ptr graph, const Params& param);
		private:
			void CullingNoUsePasses();
			void AddAssistPasses();
			void SplitAsyncCompute(RtRenderGraphExecutor::Ptr executor);
			void AnalysisResourceUsage();
			//build resource barrier
			void AddResourceTransition();
			void ValidateFinalizeGraph()const;
		private:
			RtRendererGraph::Ptr		graph_;
			Vector<RtRendererPass>		command_list_;
		};
	}
}