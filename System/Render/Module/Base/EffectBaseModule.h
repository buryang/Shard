#pragma once
#include "Utils/CommonUtils.h"
#include "Graphics/Render/RenderShader.h"
#include "Graphics/Render/RenderGraphBuilder.h"

namespace Shard::Effect
{
    class MINIT_API EffectDrawBase
    {
    public:
		//some prepare work before draw, every frame resource update etc
		virtual void PreDraw(Render::RenderGraphBuilder& builder) {};
		//all render jobs logic should execute while rendering 
    	virtual void Draw(Render::RenderGraphBuilder& builder) = 0;
		virtual void PostDraw(Render::RenderGraphBuilder& builder) {};
		virtual ~EffectDrawBase() = default;
    };
}