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
			return INVALID_INDEX;
		}

		bool RtRenderResourceBridge::IsOptimizeNeeded() const
		{
			return src_field_.size() > 1 || dst_field_.size() > 1;
		}
		
		void RtRenderResourceBridge::Optimize()
		{
		}
		bool RtRenderResourceBridge::ResourceBridgeTransitionHelper::operator()(void)
		{
			const auto& require_transition_func = [](const PinType lhs, const PinType rhs) {
				if (lhs->IsTransitionNeed(*rhs) || /*FIXME check pipeline*/) {
					return true;
				}
			};

			for (auto& spin : src_field_) {
				for (auto& dpin : dst_field_) {
					if (require_transition_func(spin, dpin)) {

					}
				}
			}
			return true;
		}
	}
}