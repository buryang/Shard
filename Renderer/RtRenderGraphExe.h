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
			using CallBack = std::function<void(RtRenderGraphExecutor& executor, PassHandle handle)>;
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
			TextureHandle GetOrCreateTexture(const RtField& field);
			BufferHandle GetOrCreateBuffer(const RtField& field);
			//binding external resource to executor
			RtRenderGraphExecutor& RegistExternalResource(const std::string& field_name, RtRenderResource::Ptr resource);
			//whether executor ready for draw
			bool IsReady()const;
		private:
			void ExecutePass(RHI::RHICommandContext* context, RtRendererPass& pass);
			//queue extracted output resource 
			RtRenderGraphExecutor& QueueExtractedTexture(RtField& field, RtRenderTexture::Ptr& texture);
			RtRenderGraphExecutor& QueueExtractedBuffer(RtField& field, RtRenderBuffer::Ptr& buffer);
			RtRenderGraphExecutor() = default;
			DISALLOW_COPY_AND_ASSIGN(RtRenderGraphExecutor);
		private:
			friend class RtRenderGraphBuilder;
			friend class RtFieldResourcePlanner;
			bool							is_compiled_{ false };
			Map<PassHandle, RtRendererPass>	passes_;
			Utils::Allocator*				allocator_{nullptr};
			Map<PassHandle, SmallVector<CallBack>>	pre_watch_dogs_;
			Map<PassHandle, SmallVector<CallBack>>	post_watch_dogs_;
			Map<String, TextureHandle>	texture_map_;
			Map<String, BufferHandle>	buffer_map_;
		};

		void GatherPassResources(RtRenderGraphExecutor& executor, PassHandle hande);

		//here my idea: combin all shared edge resource together, and do resource plan
		class RtFieldResourcePlanner
		{
		public:
			void DoResourcePlan(RtRenderGraphExecutor& executor);
			void AppendProducer(RtField& field, PassHandle pass);
			void ApeendConsumer(RtField& field, PassHandle pass);
		private:
			void AddTransition(RtRenderGraphExecutor& executor, PassHandle pass, const RtField& prev, const RtField& next);
			void AddTransition(RtRenderGraphExecutor& executor, PassHandle pass, bool is_producer, 
								const RtField::SubTextureField& prev, const RtField::SubTextureField& next);
		private:
			SmallVector<FieldNode, 2>	producers_;
			SmallVector<FieldNode, 2>	consumers_;
		};
	}
}
