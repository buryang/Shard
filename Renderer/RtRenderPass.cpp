#include "RtRenderPass.h"

namespace MetaInit
{
	namespace Renderer
	{
		RtRendererPass::RtRendererPass(const std::string& name, uint32_t index):name_(name), pass_id_(index)
		{
		}

		bool RtRendererPass::IsIsolated() const
		{
			return false;
		}

		bool RtRendererPass::IsAysnc() const
		{
			return is_async_;
		}
	}
}