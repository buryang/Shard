#pragma once

#include "Renderer/RtRenderPass.h"
#include "Renderer/RtRenderGraphExe.h"
#include "RHI/RHI.h"

namespace MetaInit
{
	namespace Renderer
	{
		class RtRendererGraph;
		class MINIT_API RtRenderGraphBuilder
		{
		public:
			typedef struct _BuildParams
			{
				bool	aync_enable_{ false };
				bool	culling_passes_{ false };
				bool	res_aliasing_enable_{ false };
			}Params;
			static RtRenderGraphExecutor::Ptr Build(RtRendererGraph& graph, const Params& param);
		private:
			RtRenderGraphBuilder(RtRendererGraph& graph);
			void CullingNoUsePasses();
			void AddAssistPasses();
			void AnalysisResourceUsage();
			//build resource barrier
			void AddResourceTransition();
			bool ValidateFinalizeGraph()const;
		private:
			RtRendererGraph&			graph_;
			//sorted filtered passes
			Vector<PassHandle>			command_list_;
		};
	}
}