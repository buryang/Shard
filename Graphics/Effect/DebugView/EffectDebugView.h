#pragma once

#if 1//def DEVELOP_DEBUG_TOOLS
#include "Utils/CommonUtils.h"
#include "System/DebugView/DebugViewSystem.h"
#include "Graphics/Renderer/RtRenderGraphBuilder.h"
#include "Graphics/RHI/RHIShaderLibrary.h"
#include "Graphics/RHI/RHICommand.h"

namespace Shard::Effect
{
	class MINIT_API DebugViewRender
	{
	public:
		enum
		{
			LINE_TYPE = 0,
			POINT_TYPE = 1,
			MAX_DEBUG_PRIMIVE_TYPE = 2,
		};
		static void Init();
		static void UnInit();
		void Render(System::DebugView::DebugViewContext& ctx, RHI::RHICommandContext::Ptr cmd_buffer);
	private:
		void DrawLines(const SmallVector<System::DebugView::LineViewCommand>& lines, RHI::RHICommandContext::Ptr cmd_buffer);
		void DrawPoints(const SmallVector<System::DebugView::PointViewCommand>& points, RHI::RHICommandContext::Ptr cmd_buffer);
	private:
		static Array<RHI::RHIPipelineStateObject::Ptr, MAX_DEBUG_PRIMIVE_TYPE>	debug_pso_;
	};

	class MINIT_API EffectDrawDebugView
	{
	public:
		static void Init();
		static void Unit();
		static void Draw(Renderer::RtRenderGraphBuilder& builder, System::DebugView::DebugViewSystem& debugview);
	};
}

#endif
