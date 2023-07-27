#pragma once
#include "Utils/Algorithm.h"
#include "Utils/Platform.h"
#include "Scene/Scene.h"

/*render thread main entrance and logic*/
namespace Shard::Renderer
{
	class MINIT_API RenderSystem
	{
	public:
		static void Init();
		static void Unit();
		static void Tick(float dt);
		static void SetWindow(Utils::WindowHandle win);
	private:
		RenderSystem() = default;
		DISALLOW_COPY_AND_ASSIGN(RenderSystem);
	};
}