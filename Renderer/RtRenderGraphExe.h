#pragma once

#include "Utils/CommonUtils.h"
#include "RHI/RHI.h"

namespace MetaInit
{
	namespace Renderer
	{
		//think about whether new rhi proxy
		class MINIT_API RtRenderGraphExecutor
		{
		public:
			using Ptr = std::shared_ptr<RtRenderGraphExecutor>;
			static Ptr Create();
			void Execute();
		private:
			void ExecutePass(RHIPass& pass);
			RtRenderGraphExecutor() = default;
			DISALLOW_COPY_AND_ASSIGN(RtRenderGraphExecutor);
		private:
			friend class RtRenderGraphBuilder;
			Vector<RHIPass>				passes_;
			RHICommandContext::Ptr		rhi_;
			RtTransiantResourceWatchDog	transiant_hook_;
		};
	}
}
