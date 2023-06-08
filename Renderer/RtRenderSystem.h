#pragma once
#include "Utils/Algorithm.h"

/*render thread main entrance and logic*/
namespace MetaInit::Renderer
{
	struct RtRenderCommand
	{
		using Ptr = RtRenderCommand*;
		uint32_t	flags_{ 0u };
		eastl::shared_ptr<void>	meta_info_;
	};

	class RtRenderCommandStorage
	{
	public:
		void Init(size_t capacity);
		RtRenderCommand::Ptr Get(size_t index);
		bool Set(size_t index, RtRenderCommand::Ptr job);
		void UnInit();
		~RtRenderCommandStorage() { UnInit(); }
	private:
		size_t	capacity_{ 0u };
		RtRenderCommand::Ptr*	cmd_storage_{ nullptr };
	};

	class MINIT_API RtRenderMessageQueue : public Utils::TRingBuffer<RtRenderCommand, RtRenderCommandStorage>
	{
	public:
		using Ptr = RtRenderMessageQueue*;
		static RtRenderMessageQueue::Ptr Instance();
		void Enqueue(RtRenderCommand::Ptr cmd);
		RtRenderCommand::Ptr Dequeue();
	private:
		DISALLOW_COPY_AND_ASSIGN(RtRenderMessageQueue);
	};

	class MINIT_API RtRenderSystem
	{
	public:
		static void Init();
		static void Run();
		static void UnInit();
	private:
		static std::atomic_flag	 quit_flags_;
	};
}