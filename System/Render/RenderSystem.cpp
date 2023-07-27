#include "Utils/CommonUtils.h"
#include "Core/RenderGlobalParams.h"
#include "Scene/Scene.h"
#include "Renderer/RtRenderSystem.h"


namespace Shard::Renderer {

	REGIST_PARAM_TYPE(FLOAT, RENDER_TARGET_FPS, 30);
	REGIST_PARAM_TYPE(BOOL, RENDER_SKIP_FRAME, false);
	REGIST_PARAM_TYPE(BOOL, RENDER_FIXED_FPS, false);

	void RtRenderSystem::Init()
	{
	}

	void RtRenderSystem::Unit()
	{
	}

	void RtRenderSystem::Tick(float dt)
	{
	}

}