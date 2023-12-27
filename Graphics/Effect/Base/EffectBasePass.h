#pragma once
#include "Utils/CommonUtils.h"
#include "Graphics/Renderer/RtRenderShader.h"
#include "Graphics/Renderer/RtRenderGraphBuilder.h"

namespace Shard::Effect
{
	class MINIT_API EffectDrawBase
	{
	public:
		static void Draw(Renderer::RtRenderGraphBuilder& builder);
	};
}