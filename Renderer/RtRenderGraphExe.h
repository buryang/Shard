#pragma once

#include "Utils/CommonUtils.h"
#include "RHI/RHI.h"
#include "Renderer/RtRenderPass.h"

namespace MetaInit
{
	namespace Renderer
	{
		//think about whether new rhi proxy
		class MINIT_API RtRenderGraphExecutor
		{
		public:
			using Ptr = std::shared_ptr<RtRenderGraphExecutor>;
			using CallBack = std::function<void(void)>;
			struct _CommandContext
			{
				RHICommandContext*		rhi_{ nullptr };
				RHICommandContext*		async_rhi_{ nullptr };
			};
			using RHIUnionContext = _CommandContext;
		public:
			static Ptr Create(Allocator* alloc);
			void Execute(RHIUnionContext& context);
			void InsertPass(PassHandle pass);
			void InsertCallBack(uint32_t time, CallBack&& call);
			//binding external resource to executor
			void BindExternalResource(RtField* field, );
			//whether executor ready for draw
			bool IsReady()const;
		private:
			void ExecutePass(RHICommandContext* context, RtRendererPass& pass);
			void PrepareExecutorResources();
			void RegistExtractResources();
			RtRenderGraphExecutor() = default;
			DISALLOW_COPY_AND_ASSIGN(RtRenderGraphExecutor);
		private:
			friend class RtRenderGraphBuilder;
			Vector<PassHandle>				passes_;
			Allocator*						allocator_{nullptr};
			std::unordered_map<uint32_t, SmallVector<CallBack>>	watch_dogs_;
		};
	}
}
