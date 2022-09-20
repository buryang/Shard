#include "RtRenderGraphExe.h"

namespace MetaInit
{
	namespace Renderer
	{
		RtRenderGraphExecutor::Ptr RtRenderGraphExecutor::Create()
		{
			return Ptr(new RtRenderGraphExecutor);
		}

		void RtRenderGraphExecutor::Execute(RHIUnionContext& context)
		{
			for(auto& pass:passes_)
			{
				auto curr_context = pass.IsAysnc() ? context.async_rhi_ : context.rhi_;
				//barrier begin todo 
				ExecutePass(curr_context, pass);
				//barrier end todo
			}

		}

		void RtRenderGraphExecutor::AddTransition(RtField* before, RtField* after)
		{
		}

		void RtRenderGraphExecutor::InsertPass(RtRendererPass& pass)
		{
			passes_.push_back(pass);
		}

		void RtRenderGraphExecutor::InsertCallBakc(uint32_t time, CallBack&& call)
		{
			if (watch_dogs_.find(time) == watch_dogs_.end()) {
				watch_dogs_[time] = { call };
			}
			else
			{
				watch_dogs_[time].emplace_back(call);
			}
		}

		void RtRenderGraphExecutor::ExecutePass(RtRendererPass& pass)
		{
			const auto pass_index = 0;
			pass.Execute();
			if (auto it = watch_dogs_.find(pass_index); it != watch_dogs_.end()) {
				for (auto call : it->second) {
					call();
				}
			}
		}

		static void BeginResourceRHI(RtRenderResource texture, );
		static void BeginResourceRHI(RtRenderResource buffer);
		static void EndResourceRHI(RtRenderResource texture);
		static void EndResourceRHI(RtRenderResource buffer);

		void RtRenderGraphExecutor::PrepareExecutorResources()
		{
			for (auto& pass : passes_) {

			}
		}
	}
}