#include "imgui.h"
#include "Core/EngineGlobalParams.h"
#include "System/DebugView/DebugViewSystem.h"

#if 1 //def DEVELOP_DEBUG_TOOLS

namespace Shard::System::DebugView
{
	SET_PARAM_TYPE_VAL(BOOL, DEBUG_VIEW_ENABLE_IMGUI, false);
	void DebugViewContext::DrawLine(vec3 line_start, vec3 line_end, vec4 color, float thickness)
	{
		new(line_batcher_) LineViewCommand{ .start_pos_ = line_start, .end_pos_ = line_end, .color_ = color };
	}

	void DebugViewContext::DrawPoint(vec3 position, vec4 color, float size)
	{
		new(point_batcher_) PointViewCommand{ .pos_ = position, .color_ = color, .thickness_ = size };
	}

	void DebugViewContext::DrawCircle(vec3 center, uint32_t segments, vec4 quat, vec4 color, float radius, float thickness)
	{
		const float sector_angle = 2 * M_PI / segments;
		vec3 line_start{ 0.f, }, line_end{ radius * cosf(sector_angle), radius * sinf(sector_angle), 0.f }; ////todo z-coord
		for (auto n = 0; n < segments; ++n)
		{
			DrawLine(line_start, line_end, color, thickness);
			std::swap(line_start, line_end);
			line_end = { radius * cosf((n + 1) * sector_angle), radius * sinf((n + 1) * sector_angle), 0.f };
		}
	}

	void DebugViewContext::DrawBox(vec3 center, vec3 size, vec4 quat, vec4 color, float thickness)
	{
		Array<vec3, 8> box_coords;
		//todo calc coordinates

		for (auto n = 0; n < 5; n += 4)
		{
			DrawLine(box_coords[0 + n], box_coords[1 + n], color, thickness);
			DrawLine(box_coords[1 + n], box_coords[2 + n], color, thickness);
			DrawLine(box_coords[2 + n], box_coords[3 + n], color, thickness);
			DrawLine(box_coords[3 + n], box_coords[0 + n], color, thickness);
		}

		for (auto n = 0; n < 4; ++n)
		{
			DrawLine(box_coords[n], box_coords[4 + n], color, thickness);
		}
	}

	void DebugViewContext::DrawSphere(vec3 center, uint32_t x_segments, uint32_t y_segments, vec4 color, float radius, float thickness)
	{
	}

	void DebugViewContext::DrawFrustum(const mat4 frustum_to_world, vec4 color, float thickness)
	{
		Array<vec3, 8> frustum_coords;
		frustum_coords[0] = frustum_to_world * vec4{ -1, 1, 0, 1 };
		frustum_coords[1] = frustum_to_world * vec4{ 1, 1, 0, 1 };
		frustum_coords[2] = frustum_to_world * vec4{ 1, -1, 0, 1 };
		frustum_coords[3] = frustum_to_world * vec4{ -1, -1, 0, 1 };
		frustum_coords[4] = frustum_to_world * vec4{ -1, 1, 1, 1 };
		frustum_coords[5] = frustum_to_world * vec4{ 1, 1, 1, 1 };
		frustum_coords[6] = frustum_to_world * vec4{ 1, -1, 1, 1 };
		frustum_coords[7] = frustum_to_world * vec4{ -1, -1, 1, 1 };

		//draw lines
		for (auto n = 0; n < 5; n += 4)
		{
			DrawLine(frustum_coords[0 + n], frustum_coords[1 + n], color, thickness);
			DrawLine(frustum_coords[1 + n], frustum_coords[2 + n], color, thickness);
			DrawLine(frustum_coords[2 + n], frustum_coords[3 + n], color, thickness);
			DrawLine(frustum_coords[3 + n], frustum_coords[0 + n], color, thickness);
		}

		for (auto n = 0; n < 4; ++n)
		{
			DrawLine(frustum_coords[n], frustum_coords[4 + n], color, thickness);
		}

	}

	void DebugViewContext::Reset()
	{
		line_batcher_.clear();
		point_batcher_.clear();
	}

	DebugViewContext& DebugViewContext::operator+=(DebugViewContext&& rhs)
	{
		eastl::move(rhs.line_batcher_.begin(), rhs.line_batcher_.end(), eastl::back_inserter(line_batcher_));
		eastl::move(rhs.point_batcher_.begin(), rhs.point_batcher_.end(), eastl::back_inserter(point_batcher_));
		rhs.Reset();
		return *this;
	}

	void DebugViewSystem::Init()
	{
#ifdef ENABLE_IMGUI
		if (GET_PARAM_TYPE_VAL(BOOL, DEBUG_VIEW_ENABLE_IMGUI)) {
			RHI::RHIGlobalEntity::Instance()->GetImGuiLayerWrapper()->Init();
		}
#endif
	}

	void DebugViewSystem::UnInit()
	{
#if 1//def ENABLE_IMGUI
		if (GET_PARAM_TYPE_VAL(BOOL, DEBUG_VIEW_ENABLE_IMGUI)) {
			RHI::RHIGlobalEntity::Instance()->GetImGuiLayerWrapper()->UnInit();
		}
#endif
		debug_context_.Reset();
	}

	void DebugViewSystem::PreUpdate()
	{
#if 1//def ENABLE_IMGUI
		RHI::RHIGlobalEntity::Instance()->GetImGuiLayerWrapper()->NewFrameGameThread();
#endif
	}

	void DebugViewSystem::MergeThreadContext(DebugViewContext&& thread_context)
	{
		static std::atomic_bool context_lock;
		Utils::SpinLock lock(context_lock);
		debug_context_ += std::move(thread_context);
	}
}
#endif