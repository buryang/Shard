#include "Renderer/RtRenderGraph.h"

namespace MetaInit
{
	namespace Renderer
	{
		void RtRendererGraph::AddPass(RtRendererPass::Ptr pass, const String& pass_name)
		{

		}
		void RtRendererGraph::RemovePass(const String& pass_name)
		{
		}
		uint32_t RtRendererGraph::GetPassIndex(const String& pass_name)const
		{
			auto iter = pass_to_index_.find(pass_name);
			if (iter != pass_to_index_.end()) {
				return iter->second;
			}
			return INVALID_INDEX;
		}
	}
}