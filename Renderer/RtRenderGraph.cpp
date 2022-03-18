#include "Renderer/RtRenderGraph.h"

namespace MetaInit
{
	namespace Renderer
	{
		uint32_t RtRendererGraph::GetPassIndex(const std::string& pass_name)
		{
			auto iter = pass_to_index_.find(pass_name);
			if (iter != pass_to_index_.end()) {
				return iter->second;
			}
			return invalid_id;
		}
	}
}