#include "RtRenderPass.h"

namespace MetaInit
{
	namespace Renderer
	{
		RtRendererPass::RtRendererPass(const std::string& name, uint32_t index):name_(name), pass_id_(index)
		{
		}
	}
}