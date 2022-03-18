#pragma once
#include "Renderer/RtRenderResources.h"
#include "Renderer/RtRenderPass.h"
#include "Utils/CommonUtils.h"
#include "Utils/DirectedAcyclicGraph.h"
#include "RHI/VulkanPrimitive.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace MetaInit
{
	namespace Renderer
	{
		constexpr uint32_t invalid_id = -1;
		template <class ResourceHandle>
		class RtRenderResource;
		class RtRendererPass;
		using namespace MetaInit::Utils;
		class MINIT_API RtRenderererGraph : public DirectedAcyclicGraph<RtRendererPass, RtField>
		{
		public:
			using Ptr = std::shared_ptr<RtRendererGraph>;
			virtual ~RtRendererGraph() = default;
			void AddPass(RtRendererPass::Ptr pass, const std::string& pass_name);
			void RemovePass(const std::string& pass_name);
			void AddField(RtRenderResource<>::Ptr texture, const std::string& field_name);
			void AddField(RtRenderResource<>::Ptr buffer, const std::string& field_name);
			uint32_t PassNum()const { return pass_to_index_.size(); }
			uint32_t GetPassIndex(const std::string& pass_name);
		private:
			DISALLOW_COPY_AND_ASSIGN(RtRendererGraph);
		private:
			friend class RtRenderGraphBuilder;
			std::unordered_map<std::string, uint32_t>	resource_to_index_;
			std::unordered_map<std::string, uint32_t>	pass_to_index_;
			bool										recompile_needed_{ true };
			SmallVector<RtField>						outputs_;
			//external resource 

		};
	}
}
