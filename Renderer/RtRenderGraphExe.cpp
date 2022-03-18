#include "RtRenderGraphExe.h"

namespace MetaInit
{
	namespace Renderer
	{
		RtRenderGraphExecutor::Ptr RtRenderGraphExecutor::Create()
		{
			return Ptr(new RtRenderGraphExecutor);
		}

		void RtRenderGraphExecutor::Execute()
		{
			for(auto& pass:passes_)
			{
				ExecutePass(pass);
			}

		}

		void RtRenderGraphExecutor::ExecutePass(RHIPass& pass)
		{
			const auto pass_index = 0;
			transiant_hook_.ExecutePass(pass_index);
		}
	}
}