#include "Renderer/RtRenderGraphBuilder.h"
#include "Utils/DirectedAcyclicGraph.h"

namespace MetaInit
{
	namespace Renderer
	{

		void RtRenderGraphBuilder::AnalysisResourceUsage(const RtRendererGraph& graph, RtRenderGraphExecutor::Ptr executor)
		{
			/*all resource field manager buffer*/
			Map<String, RtFieldResourcePlanner> resource_planers;

			for (auto& [pass_handle, pass] : executor->passes_) {
				auto& fields = pass.GetSchduleContext().GetFields();
				for (auto& [_, field] : fields) {
					/*render graph do not dist external/unreferenced resource*/
					if (field.IsExternal()||!field.IsReferenced()) {
						continue;
					}
					auto& parent_name = field.GetParentName();
					if (resource_planers.find(parent_name) == resource_planers.end()) {
						resource_planers.insert(eastl::make_pair(parent_name, RtFieldResourcePlanner()));
					}

					auto& planner = resource_planers[parent_name];
					if (field.IsOutput()) {
						planner.AppendProducer(field, pass_handle);
					}
					else
					{
						planner.ApeendConsumer(field, pass_handle);
					}
				}
			}

			//do all resource planer work
			for (auto& [_, planner] : resource_planers) {
				planner.DoResourcePlan(*executor);
			}

		}

		void RtRenderGraphBuilder::AddResourceTransition(RtRenderGraphExecutor::Ptr executor)
		{
			for () {
				executor->InsertCallBack();
			}
		}

		void RtRenderGraphBuilder::AddResourceTransition(RtRenderGraphExecutor::SharedPtr executor)
		{
			for (auto& [pass_handle, pass] : executor->passes_) {
				//insert prologure pass barrier
				executor->InsertCallBack(pass_handle, [](auto& executor) {

					}, false);
				//insert epilogure pass barrier
				executor->InsertCallBack(pass_handle, &](auto& executor){
				}, true);
			}
		}

		bool RtRenderGraphBuilder::ValidateFinalizeGraph(const RtRendererGraph& graph)
		{
			//first of check whether a direct acyclic graph
			if(!graph.IsValid())
			{
				return false;
			}
			//check other things
			return true;
		}

		RtRenderGraphExecutor::Ptr RtRenderGraphBuilder::Compile(RtRendererGraph& graph, const RtRendererGraph::BuildConfig& param)
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
			RtRenderGraphExecutor::SharedPtr executor(new RtRenderGraphExecutor);
			std::sort(builder.command_list_.begin(), builder.command_list_.end());
			for (auto& pass_handle : builder.command_list_) {
				executor->InsertPass(pass_handle);
			}

			//4. do resource analysis
			builder.AnalysisResourceUsage(graph_, executor);

			executor->is_compiled_ = true;
			return executor;
		}

		void RtRenderGraphBuilder::CullingNoUsePasses(RtRendererGraph& graph)
		{
			//track outputs related passes
			for (auto n = 0; n < graph.OutputNum(); ++n) {
				Utils::DirectGraphVisitor<Utils::DFSSearch, decltype(graph)> visitor(graph, );
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

