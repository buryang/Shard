#include <fstream>
#include "folly/dynamic.h"
#include "folly/json.h"
#include "System/Render/RuntimeHDRPRenderer.h"

namespace Shard
{
        namespace Runtime
        {
                void RuntimeHDRPRenderer3D::RendererCfg::Load(const std::string& path)
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

                void RuntimeHDRPRenderer3D::RendererCfg::Dump(const std::string& path)
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

                void RuntimeHDRPRenderer3D::RenderScene()
                {
                        //begin render main logic here

                }

                const RendererCfg& RuntimeHDRPRenderer3D::Config() const
                {
                        return config_;
                }

                bool RuntimeHDRPRenderer3D::GatherDyamicMeshDrawCommand(Renderer::RtRenderGraphBuilder& builder)
                {
                        return false;
                }

                bool RuntimeHDRPRenderer3D::GatherRayTracingWorld(Renderer::RtRenderGraphBuilder& builder)
                {
                        return false;
                }


        }
}

