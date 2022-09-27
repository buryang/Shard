#include "Renderer/RtRenderGraphExe.h"
#include "RHI/RHI.h"

namespace MetaInit
{
	namespace Renderer
	{

		RtRenderGraphExecutor::Ptr RtRenderGraphExecutor::Create(Utils::Allocator* alloc)
		{
			Ptr executor(new RtRenderGraphExecutor);
			executor->allocator_ = alloc;
			return executor;
		}

		void RtRenderGraphExecutor::Execute(RHIUnionContext& context)
		{
			for(auto& pass:passes_)
			{
				auto curr_context = pass.IsAysnc() ? context.async_rhi_ : context.rhi_;
				for(auto& call_back: pre_watch_dogs_[pass]) {
					call_back(*this, pass);
				}
				ExecutePass(curr_context, pass);
				for (auto& call_back : post_watch_dogs_[pass]) {
					call_back(*this, pass);
				}
			}

		}

		void RtRenderGraphExecutor::InsertPass(PassHandle pass)
		{
			passes_.emplace_back(pass);
			//default insert collect resource to callbacks
			InsertCallBack(pass, CollectPassResource, false);
		}

		void RtRenderGraphExecutor::InsertCallBack(PassHandle time, CallBack&& call, bool is_post)
		{
			auto& pass_cbs = is_post ? post_watch_dogs_[time] : pre_watch_dogs_[time];
			pass_cbs.emplace_back(call);
		}

		RtRenderGraphExecutor& RtRenderGraphExecutor::RegistExternalResource(const std::string& field_name, RtRenderResource::Ptr resource)
		{

			return *this;
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

		RtRenderGraphExecutor& RtRenderGraphExecutor::QueueExtractedTexture(RtField& field, RtRenderTexture::Ptr& texture)
		{
			assert(field.IsOutput()&&"extracted field should be output");
			// TODO: 在此处插入 return 语句
			return *this;
		}

		bool RtRenderGraphExecutor::IsReady() const
		{
			return is_compiled_;
		}

		void RtRenderGraphExecutor::ExecutePass(RHI::RHICommandContext* context, RtRendererPass& pass)
		{
		}

		void RtRenderGraphExecutor::PrepareExecutorResources()
		{
			for (auto& pass : passes_) {

			}
		}
		void RtRenderGraphExecutor::QueueExtractResources()
		{
		}

		void GatherPassResources(RtRenderGraphExecutor& executor, PassHandle hande)
		{
		}
}
}