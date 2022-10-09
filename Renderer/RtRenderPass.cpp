#include "RtRenderPass.h"

namespace MetaInit
{
	namespace Renderer
	{
		RtRendererPass::RtRendererPass(const String& name, const EPipeLine pipeline):name_(name), pipeline_(pipeline)
		{
		}

		RtRendererPass& RtRendererPass::SetScheduleContext(ScheduleContext&& context)
		{
			schedule_context_ = context;
		}

	}
}