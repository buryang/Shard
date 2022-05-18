#pragma once

#include "Utils/CommonUtils.h"
#include "RHI/RHI.h"
#include "Renderer/RtRenderPass.h"
#include "Runtime/BaseGraphicsPass.h"

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
			void CreateOrGetBuffer(RtField& buffer_field);
			void CreateOrGetTexture(RtField& texture_field);
			void CreateOrGetSRV(RtField& srv_field);
			void CreateOrGetUAV(RtField& uav_field);
		private:
			void ExecutePass(RtRendererPass& pass);
			RtRenderGraphExecutor() = default;
			DISALLOW_COPY_AND_ASSIGN(RtRenderGraphExecutor);
		private:
			friend class RtRenderGraphBuilder;
			Vector<RtRendererPass>		passes_;
			RHICommandContext::Ptr		rhi_;
			RtTransiantResourceWatchDog	transiant_hook_;
			Utils::Allocator			allocator_;
		};
	}
}
