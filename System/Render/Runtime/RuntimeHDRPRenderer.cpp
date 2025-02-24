#include <fstream>
#include "folly/dynamic.h"
#include "folly/json.h"
#include "RuntimeHDRPRenderer.h"

namespace Shard::Render
{
    namespace Runtime
    {
        void RuntimeHDRPRender3D::RenderCfg::Load(const std::string& path)
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

        void RuntimeHDRPRender3D::RenderCfg::Dump(const std::string& path)
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

        void RuntimeHDRPRender3D::RenderScene()
        {
            //begin render main logic here

        }

        const auto& RuntimeHDRPRender3D::Config() const
        {
            return config_;
        }

        bool RuntimeHDRPRender3D::GatherDyamicMeshDrawCommand(Render::RenderGraphBuilder& builder)
        {
            return false;
        }

        bool RuntimeHDRPRender3D::GatherRayTracingWorld(Render::RenderGraphBuilder& builder)
        {
            return false;
        }


    }
}

