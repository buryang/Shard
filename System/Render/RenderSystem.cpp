#include "Utils/CommonUtils.h"
#include "Core/RenderGlobalParams.h"
#include "Scene/Scene.h"
#include "System/Render/RenderSystem.h"


namespace Shard::Renderer {

	REGIST_PARAM_TYPE(FLOAT, RENDER_TARGET_FPS, 30);
	REGIST_PARAM_TYPE(BOOL, RENDER_SKIP_FRAME, false);
	REGIST_PARAM_TYPE(BOOL, RENDER_FIXED_FPS, false);
	REGIST_PARAM_TYPE(BOOL, RENDER_DELAY_DEL, false);//whether delay render delete

	void RenderSystem::Init()
	{
	}

	void RenderSystem::Unit()
	{
	}

	void RenderSystem::Tick(float dt)
	{
	}

	void RenderSystem::ReloadRender()
	{
	}

}