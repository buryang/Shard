#pragma once

#include "Utils/CommonUtils.h"
#include "RHI/VulkanRHI.h"

namespace MetaInit
{
	namespace Renderer
	{

		class MINIT_API RtRenderCommand
		{

		};

		class MINIT_API RtRenderGraphExecutor
		{
		public:
			using Ptr = std::shared_ptr<RtRenderGraphExecutor>;
			void Execute();
		private:
			friend class RtRenderGraphBuilder;
			Vector<Pass>	work_queue_;
			Vector<Pass>	async_queue_;
		};
	}
}
