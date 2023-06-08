#include "Utils/CommonUtils.h"
#include "Scene/Scene.h"
#include "Renderer/RtRenderLoop.h"

namespace MetaInit::Renderer {
	/*global variable define here*/
	RtRenderSystemLoop::quit_flags_ = 0u;

	void RtRenderCommandStorage::Init(size_t capacity) {
		cmd_storage_ = new RtRenderCommand::Ptr[capacity];
		capacity_ = capacity;
	}

	RtRenderCommand::Ptr RtRenderCommandStorage::Get(size_t index) {
		assert(index < capacity_);
		return cmd_storage_[index];
	}

	bool RtRenderCommandStorage::Set(size_t index, RtRenderCommand::Ptr job) {
		assert(index < capacity_);
		cmd_storage_[index] = job;
		return true;
	}

	void RtRenderCommandStorage::UnInit() {
		delete [] cmd_storage_;
		capacity_ = 0u;
	}

	void RtRenderSystemLoop::Init()
	{
	}

	void RtRenderSystemLoop::Run()
	{

		while (true) {
			auto* render_cmd = RtRenderMessageQueue::Instance()->Dequeue();
			//distribute task
		}
	}

	void RtRenderSystemLoop::UnInit()
	{
	}

	RtRenderMessageQueue::Ptr RtRenderMessageQueue::Instance()
	{
		return RtRenderMessageQueue::Ptr();
	}

}