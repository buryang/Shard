#include "eastl/sort.h"
#include "Renderer/RtRenderGraphExe.h"
#include "RHI/RHI.h"

namespace MetaInit
{
	namespace Renderer
	{
		/*gfx queue and async compute queue*/
		static constexpr uint32_t MAX_QUEUE_COUNT = 2;
		struct TextureSubFieldState
		{
			RtField::EAccessFlags	state_;
			PassHandle	first_pass_[MAX_QUEUE_COUNT];
			PassHandle	last_pass_[MAX_QUEUE_COUNT];
			TextureSubFieldState() {
				for (auto n = 0; n < MAX_QUEUE_COUNT; ++n) {
					first_pass_[n] = last_pass_[n] = (PassHandle)-1;
				}
			}
		}; 

		static void InitWholeTextureSubStates(const RtField::EFlags& init_state, SmallVector<TextureSubFieldState>& states) {
			states[0].state_ = init_state;
			states.resize(1);
		}

		static void InitAllTextureSubStates(const TextureLayout& layout, const RtField::EFlags& init_state, SmallVector<TextureSubFieldState>& states) {
			const auto states_count = TextureSubRange(layout).GetSubRangeIndexCount();
			states.resize(states_count);
			for (auto n = 0; n < states_count; ++n) {
				states[n].state_ = init_state;
			}
		}
		
		static FORCE_INLINE uint32_t TextureSubRangeIndexToVectorIndex(const TextureLayout& layout, const TextureSubRangeIndex& index) {
			return index.mip_ + index.layer_ * layout.mip_slices_ + index.plane_ * layout.mip_slices_ * layout.array_slices_;
		}

		RtRenderGraphExecutor::SharedPtr RtRenderGraphExecutor::Create(Utils::Allocator* alloc)
		{
			SharedPtr executor(new RtRenderGraphExecutor);
			executor->allocator_ = alloc;
			return executor;
		}

		void RtRenderGraphExecutor::Execute(RHIUnionContext& context)
		{
			for(auto& [handle, pass]:passes_)
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

		void RtRenderGraphExecutor::InsertPass(PassHandle pass_handle, RtRendererPass& pass)
		{
			PassData pass_data{pass_handle, pass};
			passes_.emplace_back(pass_data);
			pass.PreExecute();
		}

		const RtRendererPass& RtRenderGraphExecutor::GetRenderPass(PassHandle handle) const
		{
			auto pass_iter = eastl::find_if(passes_.begin(), passes_.end(), [&](auto data) {
											return data.first == handle; });
			assert(pass_iter != passes_.end());
			return pass_iter->second;
		}

		TextureHandle RtRenderGraphExecutor::GetOrCreateTexture(const RtField& field)
		{
			const String& key = field.GetParentName();
			if (texture_map_.find(key) == texture_map_.end()) {
				//transform field to desc
				TextureHandle ptr = AllocNoDestruct();
				texture_map_.insert(eastl::make_pair(key, ptr));
			}
			return texture_map_[key];
		}

		BufferHandle RtRenderGraphExecutor::GetOrCreateBuffer(const RtField& field)
		{
			const String& key = field.GetParentName();
			if (buffer_map_.find(key) == buffer_map_.end()) {
				//transform field to desc
				BufferHandle ptr = AllocNoDestruct();
				buffer_map_.insert(eastl::make_pair(key, ptr));
			}
			return buffer_map_[key];
		}

		RtRenderGraphExecutor& RtRenderGraphExecutor::RegistExternalResource(const String& field_name, RtRenderResource::Ptr resource)
		{

			return *this;
		}

		void RtRenderGraphExecutor::InsertCallBack(PassHandle time, CallBack&& call, bool is_post)
		{
			auto& watch_dogs = is_post ? post_watch_dogs_ : pre_watch_dogs_;
			if (watch_dogs.find(time) == watch_dogs.end()) {
				watch_dogs[time] = { call };
			}
			else
			{
				watch_dogs[time].emplace_back(call);
			}
		}

		void RtRenderGraphExecutor::ExecutePass(RHI::RHICommandContext* context, RtRendererPass& pass)
		{
			pass.Execute(context);
		}

		RtRenderGraphExecutor& RtRenderGraphExecutor::QueueExtractedTexture(RtField& field, RtRenderTexture::Ptr& texture)
		{
			assert(field.IsOutput()&&"extracted field should be output");
			auto texture_handle = GetOrCreateTexture(field);
			auto& texture = ...
			texture->SetRHI();
			return *this;
		}

		bool RtRenderGraphExecutor::IsReady() const
		{
			return is_compiled_;
		}

		void RtFieldResourcePlanner::DoResourcePlan(RtRenderGraphExecutor& executor)
		{
			if (producers_[0].field_.GetType() == RtField::EType::eBuffer) {
				DoBufferPlan(executor);
			}
			else{
				DoTexturePlan(executor);
			}
		}

		void RtFieldResourcePlanner::DoTexturePlan(RtRenderGraphExecutor& executor)
		{
			SmallVector<TextureSubFieldState> merged_states;
			//first sort producers and consumter
			SmallVector<FieldNode, 4> combined_nodes;

			combined_nodes.insert(combined_nodes.end(), producers_.begin(), producers_.end());
			combined_nodes.insert(combined_nodes.end(), consumers_.begin(), consumers_.end());
			eastl::sort(combined_nodes.begin(), combined_nodes.end());

			auto is_pipeline_equal = [&](PassHandle lhs, PassHandle rhs) {
				auto lhs_pipe = executor.GetRenderPass(lhs).GetPipeLine();
				auto rhs_pipe = executor.GetRenderPass(rhs).GetPipeLine();
				return lhs_pipe == rhs_pipe;
			};


			auto iter = combined_nodes.begin();
			auto prologue_pass = iter->pass_;
			RtField prologue_field{ (*iter++).field_ };
			const auto& field_layout = prologue_field.GetLayout();
			if (prologue_field.IsWholeResource()) {
				InitWholeTextureSubStates(prologue_field.GetSubField().access_, merged_states);
			}
			else {
				//todo fixme pass handle assign
				InitAllTextureSubStates(prologue_field.GetLayout(), prologue_field.GetSubField().access_, merged_states);
			}

			auto texture_handle = executor.GetOrCreateTexture(prologue_field);
			//insert prologue resource release handle
			executor.InsertCallBack(prologue_pass, [&]() {
					return;
				}, false);

			for (; iter != combined_nodes.end(); ++iter) {
				auto curr_pass_handle = iter->pass_;
				auto curr_pass = executor.GetRenderPass(curr_pass_handle);
				//range change firstly if needed
				if (merged_states.size() == 1 && !iter->field_.GetSubRange().IsWholeRange(prologue_field.GetLayout())) {
					InitAllTextureSubStates(prologue_field.GetLayout(), merged_states[0].state_, merged_states);
				}
				iter->field_.GetSubRange().For([&](const auto& range_index) {
					auto merge_index = TextureSubRangeIndexToVectorIndex(field_layout, range_index);
					auto& merge_state = merged_states[merge_index];
					const auto& state = iter->field_.GetSubField().access_;
					if (RtField::IsSubFieldMergeAllowed(merge_state, state)) {
						merge_state = LogicOrFlags(merge_state, state);
						merged_states[merge_index].
					}
					if (RtField::IsSubFieldTransitionNeeded(merge_state, state)) {
						AddTransition();
					}
				});
			}
			//insert epilogue resource release handle
			{
				executor.InsertCallBack(, [&]() {
					return;
				}, true);
			}

		}
		void RtFieldResourcePlanner::DoBufferPlan(RtRenderGraphExecutor& executor)
		{

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

}
}