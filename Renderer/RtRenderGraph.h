#pragma once
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
		class RtField;
		class RtRendererPass;
		class MINIT_API RtRendererGraph 
		{
		public:
			using Ptr = std::shared_ptr<RtRendererGraph>;
			virtual ~RtRendererGraph() = default;
			void LoadFromJson(const std::string& json);
			void ExportToJson(const std::string& json)const;
			void AddNode();
			void RemoveNode();
			void AddEdge();
			void RemoveEdge();
			void ResizeWindow();
			uint32_t PassNum()const { return passes_.size(); }
		private:
			DISALLOW_COPY_AND_ASSIGN(RtRendererGraph);
		private:
			friend class RtRenderGraphBuilder;
			std::unordered_map<std::string, uint32_t>	resource_to_index_;
			std::unordered_map<std::string, uint32_t>	pass_to_index_;
			Vector<RtRendererPass>						passes_;
			Vector<RtField>								resources_;
			bool										recompile_needed_{ true };
			Utils::DirectedAcyclicGraph					graph_;
		};
	}
}
