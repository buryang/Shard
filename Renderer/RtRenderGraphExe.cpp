#include "eastl/sort.h"
#include "Renderer/RtRenderGraphExe.h"
#include "RHI/RHI.h"

namespace MetaInit
{
	namespace Renderer
	{

		RtRenderGraphExecutor::SharedPtr RtRenderGraphExecutor::Create(Utils::Allocator* alloc)
		{
			SharedPtr executor(new RtRenderGraphExecutor);
			executor->allocator_ = alloc;
			return executor;
		}

		void RtRenderGraphExecutor::Execute(RHIUnionContext& context)
		{
			for(auto handle:passes_)
			{
				auto& pass = ..
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

		const RtRendererPass& RtRenderGraphExecutor::GetRenderPass(PassHandle handle) const
		{
			return passes_[handle];
		}

		TextureHandle RtRenderGraphExecutor::GetOrCreateTexture(const RtField& field)
		{
			return TextureHandle();
		}

		BufferHandle RtRenderGraphExecutor::GetOrCreateBuffer(const RtField& field)
		{
			return BufferHandle();
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

		RtRenderGraphExecutor& RtRenderGraphExecutor::QueueExtractedTexture(RtField& field, RtRenderTexture::Ptr& texture)
		{
			assert(field.IsOutput()&&"extracted field should be output");
			auto texture_handle = GetOrCreateTexture(field);
			auto& texture = ...
			texture->
			return *this;
		}

		bool RtRenderGraphExecutor::IsReady() const
		{
			return is_compiled_;
		}

		void RtRenderGraphExecutor::ExecutePass(RHI::RHICommandContext* context, RtRendererPass& pass)
		{
		}

		void GatherPassResources(RtRenderGraphExecutor& executor, PassHandle hande)
		{
		}

		static void InitAllSubStates(const RtField& field, SmallVector<RtField::SubTextureField>& sub_states)
		{
			auto states_count = field.GetSubFieldCount();
			sub_states.resize(states_count);
			if (field.IsWholeResource()) {
				for (auto& state : sub_states) {
					state = field[0];
				}
			}
			else
			{
				for (auto n = 0; n < states_count; ++n) {
					sub_states[n] = field[n];//fixme
				}
			}
		}

		void RtFieldResourcePlanner::DoResourcePlan(RtRenderGraphExecutor& executor)
		{
			//first sort producers and consumter
			SmallVector<FieldNode, 4> combined_nodes;
			SmallVector<RtField::SubTextureField> merged_states;

			//eastl::sort(producers_.begin(), producers_.end());
			//eastl::sort(consumers_.begin(), consumers_.end());
			combined_nodes.insert(combined_nodes.end(), producers_.begin(), producers_.end());
			combined_nodes.insert(combined_nodes.end(), consumers_.begin(), consumers_.end());
			eastl::sort(combined_nodes.begin(), combined_nodes.end());
			
			auto is_pipeline_equal = [&](PassHandle lhs, PassHandle rhs) {
				auto lhs_pipe = executor.GetRenderPass(lhs).GetPipeLine();
				auto rhs_pipe = executor.GetRenderPass(rhs).GetPipeLine();
				return lhs_pipe == rhs_pipe;
			};

			/*init merged states with the first rtfield*/
			InitAllSubStates(combined_nodes[0].field_, merged_states);

			auto iter = combined_nodes.begin();
			auto prologue_pass = iter->pass_;
			RtField prologue_field{(*iter++).field_};
			for (; iter != combined_nodes.end(); ++iter) {
				if (!prologue_field.IsMergeAble(iter->field_)||is_pipeline_equal(prologue_pass, iter->pass_)) {
					break;
				}
				prologue_field.Merge(iter->field_);
			}
			
			if (prologue_field.GetType() == RtField::EType::eBuffer) {
				auto handle = executor.GetOrCreateBuffer(prologue_field);
				executor.InsertCallBack();//insert rhi create handle
				//register field resource handle
				executor.buffer_map_.insert(eastl::make_pair(iter->field_.GetParentName(), handle));
			}
			else {
				auto handle = executor.GetOrCreateTexture(prologue_field);
				executor.InsertCallBack();//fixme rhi create handle
				executor.texture_map_.insert(eastl::make_pair(iter->field_.GetParentName(), handle));
			}

			while (iter != combined_nodes.end()) {
				auto epilogure_pass = iter->pass_;
				RtField epilogure_field{(*iter++).field_};
				for (; iter != combined_nodes.end(); ++iter) {
					if (!epilogure_field.IsMergeAble(iter->field_) || is_pipeline_equal(epilogure_pass, iter->pass_)) {
						break;
					}
				}
				AddTransition();
				prologue_field = epilogure_field;
			}


		}
		void RtFieldResourcePlanner::AppendProducer(RtField& field, PassHandle pass)
		{
			FieldNode producer{ field, pass };
			producers_.emplace_back(producer);
		}
		void RtFieldResourcePlanner::ApeendConsumer(RtField& field, PassHandle pass)
		{
			FieldNode consumer{ field, pass };
			consumers_.emplace_back(consumer);
		}
		
		void RtFieldResourcePlanner::AddInitRHI(RtRenderGraphExecutor& executor, PassHandle pass, const RtField& field)
		{
		}

		void RtFieldResourcePlanner::AddTransition(RtRenderGraphExecutor& executor, PassHandle pass, const RtField& prev, const RtField& next)
		{
		}

		void RtFieldResourcePlanner::AddTransition(RtRenderGraphExecutor& executor, PassHandle pass, bool is_producer, const RtField::SubTextureField& prev, const RtField::SubTextureField& next)
		{

		}
}
}