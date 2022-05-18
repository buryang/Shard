#include "RtRenderPass.h"

namespace MetaInit
{
	namespace Renderer
	{
		RtRendererPass::RtRendererPass(const std::string& name, const EPipeLine pipeline, uint32_t index):name_(name), pipeline_(pipeline)
		{
		}

		RtRendererPass& RtRendererPass::AddParameters(Parameters&& params)
		{
			// TODO: 在此处插入 return 语句
		}

		void RtRendererPass::RtPassParameters::AddField(RtField&& field)
		{
		}

		RtField& RtRendererPass::RtPassParameters::operator[](const uint32_t index)
		{
			assert(index < fields_.size());
			return fields_[index];
		}
	}
}