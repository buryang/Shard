#include <fstream>
#include "folly/dynamic.h"
#include "folly/json.h"
#include "Utils/Timer.h"
#include "RuntimeRenderPipline.h"

using namespace Shard::Renderer::FXR;

namespace Shard::Renderer
{
    void HDRPRender3D::RenderCfg::Load(const String& path)
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

    void HDRPRender3D::RenderCfg::Dump(const std::string& path)
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

    const auto& HDRPRender3D::Config() const
    {
        return config_;
    }

    void HDRPRender3D::Render(Span<FXR::ViewRender>& views)
    {
        BeginUpdateSceneProxyFromECS(); 
        fxr_modules_->Draw(render_graph_, views, ERenderPhase::ePreInitViews);
        //1.init views and collect meshes
        EndUpdateSceneProxyFromECS();

        BeginInitViews(views);
        BeginBuildLights();

        //render volume/procedual meshes/hair, vfs system first
        //update scene proxy gpu data, and if ray tracing enabled, update blas/tlas

        //2. pre-render 
        //after visibility state & scene UB updated, scene draw command built
        //do the GPU culling now
        culling_manager_.PreRender();
        fxr_modules_->Draw(render_graph_, views, ERenderPhase::ePreRender);

        //3. render work
        RenderInternal();
        
        //4.last call shadow map render post render
        fxr_modules_->->Draw(render_graph_, views, ERenderPhase::ePostRender);

        //do the culling post work after all render work done
        culling_manager_.PostRender();

        //build the render graph, then flush the render command to HAL
        //todo
    }

    void HDRPRender3D::BeginUpdateSceneProxyFromECS()
    {
    }

    
    void HDRPRender3D::EndUpdateSceneProxyFromECS()
    {
        sync_ecs_proxy_handle_.Wait();
        //do other post work
    }


    void HDRPRender3D::BeginInitViews(Span<FXR::ViewRender>& views)
    {
        //collector mesh draw commands

        //upload scene to GPU
    }

    void HDRPFXRArray::Draw(Render::RenderGraph& graph, Span<FXR::ViewRender>& views, ERenderPhase phase)
    {
        for (auto iter = begin(); iter != end(); ++iter)
        {
            iter->Draw(graph, views, phase);
        }
    }
}

