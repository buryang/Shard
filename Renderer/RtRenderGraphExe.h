#pragma once

#include "Utils/Memory.h"
#include "RHI/RHI.h"
#include "Renderer/RtRenderResources.h"
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
			using CallBack = std::function<void(RtRenderGraphExecutor& executor, PassHandle handle)>;
			struct _CommandContext
			{
				RHI::RHICommandContext*		rhi_{ nullptr };
				RHI::RHICommandContext*		async_rhi_{ nullptr };
			};
			using RHIUnionContext = _CommandContext;
		public:
			static Ptr Create(Utils::Allocator* alloc);
			void Execute(RHIUnionContext& context);
			void InsertPass(PassHandle pass);
			//regist resource collection and barrier batch as callback
			void InsertCallBack(PassHandle time, CallBack&& call, bool is_post);
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
			bool							is_compiled_{ false };
			Vector<PassHandle>				passes_;
			Utils::Allocator*				allocator_{nullptr};
			std::unordered_map<PassHandle, SmallVector<CallBack>>	pre_watch_dogs_;
			std::unordered_map<PassHandle, SmallVector<CallBack>>	post_watch_dogs_;
		};

		void GatherPassResources(RtRenderGraphExecutor& executor, PassHandle hande);
	}
}
