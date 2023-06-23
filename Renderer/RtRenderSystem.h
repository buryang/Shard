#pragma once
#include "Utils/Algorithm.h"

/*render thread main entrance and logic*/
namespace Shard::Renderer
{
	class MINIT_API RtRenderSystem
	{
	public:
		static void Init();
		static void Unit();
		static void Tick(float dt);
	private:
		RtRenderSystem() = default;
		DISALLOW_COPY_AND_ASSIGN(RtRenderSystem);
	};
}