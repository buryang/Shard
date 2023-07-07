#include <fstream>
#include "folly/dynamic.h"
#include "folly/json.h"
#include "MyToySimpleRenderer.h"

#define 
namespace Shard
{
	namespace Runtime
	{
		void MyToySimpleRenderer::RendererCfg::Load(const std::string& path)
		{
			std::ifstream cfg_file(path, std::ios::binary);
			if (cfg_file.is_open())
			{
				std::stringstream str_buf;
				str_buf << cfg_file.rdbuf();
				auto cfg_str = str_buf.str();
				folly::dynamic cfg_objs = folly::parseJson(cfg_str);
				//load configure
				use_async_compute_ = cfg_objs["async"].asBool();
				use_immediate_mode_ = cfg_objs["immediate"].asBool();
			}
		}

		void MyToySimpleRenderer::RendererCfg::Dump(const std::string& path)
		{
			std::ofstream cfg_file(path, std::ios::binary);
			if (cfg_file.is_open())
			{
				const folly::dynamic object = folly::dynamic::object
												("async", use_async_compute_)
												("immediate", use_immediate_mode_);
												//todo append other things
				const auto json = folly::toJson(object);
				cfg_file << json;
			}
			
		}

		void MyToySimpleRenderer::Render(SceneProxyHelper::SharedPtr scene)
		{
			//begin render main logic here

		}

		const RendererCfg& MyToySimpleRenderer::Config() const
		{
			return config_;
		}

		bool MyToySimpleRenderer::GatherDyamicMeshDrawCommand(Renderer::RtRenderGraphBuilder& builder)
		{
			return false;
		}

		bool MyToySimpleRenderer::GatherRayTracingWorld(Renderer::RtRenderGraphBuilder& builder)
		{
			return false;
		}


	}
}

