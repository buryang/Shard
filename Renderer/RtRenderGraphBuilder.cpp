#include "Renderer/RtRenderGraphBuilder.h"
#include "Utils/DirectedAcyclicGraph.h"

namespace MetaInit
{
	namespace Renderer
	{

		void RtRenderGraphBuilder::AnalysisResourceUsage()
		{
		}

		void RtRenderGraphBuilder::AddAssistPasses()
		{
		}

		void RtRenderGraphBuilder::AddResourceTransition()
		{
		}

		bool RtRenderGraphBuilder::ValidateFinalizeGraph() const
		{
			//first of check whether a direct acyclic graph
			if(!graph_.IsValid())
			{
				return false;
			}
			//check other things
			return true;
		}

		RtRenderGraphExecutor::Ptr RtRenderGraphBuilder::Build(RtRendererGraph& graph, const Params& param)
		{
			RtRenderGraphBuilder builder(graph);

			//1.step culling no use pass
			if (param.culling_passes_) {
				builder.CullingNoUsePasses();
			}

			constexpr bool is_async_compute_enable = !!param.aync_enable_;
			//2.step analysis resource transistant; add barriers
			for (auto& edge : edge_) //?
			{
			}


			//3.valid the finalize graph
			builder.ValidateFinalizeGraph();
			//last step copy execute list as ascend ordered
			RtRenderGraphExecutor::Ptr executor(new RtRenderGraphExecutor);
			std::sort(builder.command_list_.begin(), builder.command_list_.end());
			for (auto& pass_handle : builder.command_list_) {
				executor->InsertPass(pass_handle);
			}
			return executor;
		}

		RtRenderGraphBuilder::RtRenderGraphBuilder(RtRendererGraph& graph):graph_(graph)
		{

		}

		void RtRenderGraphBuilder::CullingNoUsePasses()
		{
			//track outputs related passes
			for (auto n = 0; n < graph_.OutputNum(); ++n) {
				Utils::DirectGraphVisitor<Utils::DFSSearch, decltype(graph_)> visitor(graph_, );
				while (true) {
					auto pass_node = visitor.Next();
					if (nullptr == pass_node) {
						break;
					}
					//set pass valid for 
					pass_node->SetFlags(Flags::eValid);
				}
			}
			
			//remove noused passes
			
		}

	}
}

