#pragma once

#include "Renderer/RtRenderGraphExe.h"

namespace MetaInit
{
	namespace Renderer
	{
		class RtRenderGraph;
		class RtRenderGraphBuilder
		{
		public:
			typedef struct _BuildParams
			{
				bool	aync_enable_{ false };
				bool	culling_passes_{ false };
				bool	res_aliasing_enable_{ false };
			}Params;
			RtRenderGraphExecutor::Ptr Build(RtRenderGraph& graph, const Params& param);
		private:
			void CullingNoUsePasses();
			void AddAssistPasses();
			void SplitAsyncCompute(RtRenderGraphExecutor::Ptr executor);
			void AnalysisResourceUsage();
		private:
			Vector<uint32_t>		pass_to_async_;
			Vector<uint32_t>		pass_to_culling_;
			Vector<uint32_t>		pass_never_culling_;
			Vector<uint32_t>		pass_no_params_;
		};
	}
}