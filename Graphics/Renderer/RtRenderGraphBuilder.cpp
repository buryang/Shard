#include "Graphics/Renderer/RtRenderGraphBuilder.h"
#include "Graphics/RHI/RHIGlobalEntity.h"
#include "Utils/DirectedAcyclicGraph.h"

namespace Shard
{
	namespace Renderer
	{

		void RtRenderGraphBuilder::AnalysisResourceUsage()
		{
			/*all resource field manager buffer*/
			Map<String, RtFieldResourcePlanner> resource_planers;

			for (auto& [pass_handle, pass] : graph_exe_->passes_) {
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
				planner.DoResourcePlan(*graph_exe_);
			}

		}

		void RtRenderGraphBuilder::AddResourceTransition()
		{
			for (auto& [pass_handle, pass] : graph_exe_->passes_) {
				//insert prologure pass barrier
				graph_exe_->InsertCallBack(pass_handle, [](auto& executor) {

				}, false);
				//insert epilogure pass barrier
				graph_exe_->InsertCallBack(pass_handle, [&](auto& executor){
				}, true);
			}
		}

		void RtRenderGraphBuilder::Compile(const BuildConfig& build_param)
		{
			if (!graph_.IsCompileNeeded()) {
				return; //todo check whether params are the same
			}

			//pre valid build param
			const auto valid_build_config = [](const auto& build_param)
			{
				BuildConfig param = build_param;
				if (param.aync_enable_) {
					param.aync_enable_ = RHI::RHIGlobalEntity::Instance()->IsAsyncComputeSupported();
					LOG(WARNING) << "aync compute is not supported by device, so closed";
				}
				if (param.hw_raytrace_enable_) {
					param.hw_raytrace_enable_ = RHI::RHIGlobalEntity::Instance()->IsHWRayTraceSupported();
					LOG(WARNING) << "hardware ray tracing is not supported by device, so closed";
				}
				return param;
			};

			const auto param = valid_build_config(build_param);

			//1.step culling no use pass
			if (param.culling_passes_) {
				CullingNoUsePasses();
			}

			const bool is_async_compute_enable = !!param.aync_enable_;
			//2.step analysis resource transistant; add barriers
			for (auto& edge : graph_.) //?
			{
			}


			//3.valid the finalize graph
			if (!graph_.IsValid())
			{
				LOG(ERROR) << "RenderGraph Compile failed";
			}

		}

		void RtRenderGraphBuilder::Finalize()
		{
			//4. do resource analysis
			//last step copy execute list as ascend ordered
			//std::sort(builder.command_list_.begin(), builder.command_list_.end());
			graph_exe_ = std::make_shared<RtRenderGraphExecutor>();
			for (auto& pass_handle : command_list_) {
				graph_exe_->InsertPass(pass_handle);
			}
			
			AnalysisResourceUsage();
			graph_exe_->is_compiled_ = true;
		}

		bool RtRenderGraphBuilder::IsReady()const
		{
			return graph_exe_.get() != nullptr && graph_exe_->is_compiled_;
		}

		void RtRenderGraphBuilder::CullingNoUsePasses()
		{
			//track outputs related passes
			for (auto n = 0; n < graph_.OutputNum(); ++n) {
				Utils::DirectGraphVisitor<decltype(graph_), Utils::DFSSearch> visitor(graph_, graph_.GetNode(0u));
				while (true) {
					auto pass_node = visitor.Next();
					if (nullptr == pass_node) {
						break;
					}
					//set pass valid for 
					pass_node->SetFlags(NodeData::EFlags::eValid);
				}
			}
			
			//remove noused passes

			
		}

	}
}

