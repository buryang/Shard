#pragma once

#include "Utils/Memory.h"
#include "RHI/RHI.h"
#include "Renderer/RtRenderResources.h"
#include "Renderer/RtRenderPass.h"

namespace MetaInit
{
	namespace Renderer
	{
		struct FieldNode
		{
			RtField		field_;
			PassHandle	pass_;
			bool operator<(const FieldNode& rhs) const {
				return this->pass_ < rhs.pass_;
			}
		};

		//think about whether new rhi proxy
		class MINIT_API RtRenderGraphExecutor
		{
		public:
			using Ptr = RtRenderGraphExecutor*;
			using SharedPtr = std::shared_ptr<RtRenderGraphExecutor>;
			using CallBack = std::function<void(RtRenderGraphExecutor& executor)>;
			using TextureHandle = TextureRepo<>::Handle;
			using BufferHandle = BufferRepo<>::Handle;
			struct _CommandContext
			{
				RHI::RHICommandContext*		rhi_{ nullptr };
				RHI::RHICommandContext*		async_rhi_{ nullptr };
			};
			using RHIUnionContext = _CommandContext;
		public:
			static SharedPtr Create(Utils::Allocator* alloc);
			void Execute(RHIUnionContext& context);
			void InsertPass(PassHandle handle, RtRendererPass& pass);
			//regist resource collection and barrier batch as callback
			void InsertCallBack(PassHandle time, CallBack&& call, bool is_post=false);
			const RtRendererPass& GetRenderPass(PassHandle handle) const;
			RtRenderTexture::Ptr GetRenderTexture(TextureHandle handle);
			RtRenderBuffer::Ptr GetRenderBuffer(BufferHandle handle);
			TextureHandle GetOrCreateTexture(const RtField& field);
			BufferHandle GetOrCreateBuffer(const RtField& field);
			//binding external resource to executor
			RtRenderGraphExecutor& RegistExternalResource(const RtField& field, RtRenderResource::Ptr resource);
			//whether executor ready for draw
			bool IsReady()const;
		private:
			void ExecutePass(RHI::RHICommandContext* context, RtRendererPass& pass);
			//queue extracted output resource 
			RtRenderGraphExecutor& QueueExtractedTexture(RtField& field, RtRenderTexture::Ptr& texture);
			RtRenderGraphExecutor& QueueExtractedBuffer(RtField& field, RtRenderBuffer::Ptr& buffer);
			template<class T, typename... Args>
			T* AllocNoDestruct(Args&&... args) {
				auto result = alloc_->AllocNoDestruct(args);
				return result;
			}
			RtRenderGraphExecutor(Utils::Allocator* alloc);
			DISALLOW_COPY_AND_ASSIGN(RtRenderGraphExecutor);
		private:
			friend class RtRenderGraphBuilder;
			friend class RtFieldResourcePlanner;
			using PassData = std::pair<PassHandle, RtRendererPass>;
			bool	is_compiled_{ false };
			Vector<PassData>	passes_;
			Utils::Allocator*	allocator_{nullptr};
			Map<PassHandle, SmallVector<CallBack>>	pre_watch_dogs_;
			Map<PassHandle, SmallVector<CallBack>>	post_watch_dogs_;
			TextureRepo<>	texture_repo_;
			BufferRepo<>	buffer_repo_;
			Map<String, TextureHandle>	texture_map_;
			Map<String, BufferHandle>	buffer_map_;
			
		};

		class TextureSubFieldState;
		//here my idea: combin all shared edge resource together, and do resource plan
		class RtFieldResourcePlanner
		{
		public:
			RtFieldResourcePlanner() = default;
			void DoResourcePlan(RtRenderGraphExecutor& executor);
			void AppendProducer(RtField& field, PassHandle pass);
			void ApeendConsumer(RtField& field, PassHandle pass);
		private:
			void DoTexturePlan(RtRenderGraphExecutor& executor);
			void DoBufferPlan(RtRenderGraphExecutor& executor);
			void AddTransition(RtRenderGraphExecutor& executor, TextureSubFieldState& state_before, TextureSubFieldState state_after);
		private:
			SmallVector<FieldNode, 2>	producers_;
			SmallVector<FieldNode, 2>	consumers_;
		};
	}
}
