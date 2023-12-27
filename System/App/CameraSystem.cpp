#include "Graphics/Core/EngineGlobalParams.h"
#include "Scene/Scene.h"
#include "System/App/CameraSystem.h"

namespace Shard::System::Camera {

     REGIST_PARAM_TYPE(UINT, CAMERA_PROJ_TYPE, ECSCameraComponent::EType::ePerspective);
     REGIST_PARAM_TYPE(BOOL, CAMERA_REVERSE_Z, true);

     void ECSCameraComponent::ComputeMatrix(bool compute_proj)
     {
          view_ = glm::lookAtRH(eye_, center_, up_); 
          inv_view_ = glm::inverse(view_);
          if (compute_proj) {
               if (IsPerspective())
               {
                    proj_ = glm::perspective(fov_, view_port_.wh_.x/view_port_.wh_.y, znear_, zfar_);
               }
               else
               {
                    assert(0 && "not support orthographic projection now");
                    proj_ = glm::ortho(view_port_.xy_.x, view_port_.xy_.x+view_port_.wh_.x, view_port_.xy_.y + view_port_.wh_.y, view_port_.xy_.y); //todo
               }

               /*
                * https://tomhultonharrop.com/mathematics/graphics/2023/08/06/reverse-z.html
                * when use reverse z, remember:
                * 1.Clear depth to 0 (not 1 as usual).
                * 2.Set depth test to greater(not less as usual).
                * 3.Ensure you¡¯re using a floating point depth buffer(e.g.GL_DEPTH_COMPONENT32F, DXGI_FORMAT_D32_FLOAT_S8X24_UINT, MTLPixelFormat.depth32Float etc.)
                */
               if (IsReverseZ()) {
                    constexpr mat4 mclip{ 1.f, 0.f, 0.f, 0.f,
                                               0.f, -1.f, 0.f, 0.f,
                                               0.f, 0.f, .5f, 0.f,
                                               0.f, 0.f, .5f, 1.f };
                    proj_ = mclip * proj_;
               }
               inv_proj_ = glm::inverse(proj_);
          }
     }
     void ECSCameraComponent::Update(mat3 rotation)
     {
          eye_ = rotation * eye_;
          up_ = rotation * up_;
          ComputeMatrix();
     }

     void FPSCameraMovementSystem::Init()
     {
     }

     void FPSCameraMovementSystem::UnInit()
     {
     }

     void FPSCameraMovementSystem::Update(Utils::ECSSystemUpdateContext& ctx)
     {
          const auto& mouse_state = ctx;//todo
          auto* camera = static_cast<Scene::WorldScene*>(ctx.admin_)->GetSingletonComponent<ECSCameraComponent>();
          //get mouse delta movement
          if (mouse_state.mouse_pos_deta_) {
               const auto rotation = glm::diagonal3x3({ mouse_state.mouse_pos_deta_.x, mouse_state.mouse_pos_deta_.y, 1.f }); //todo
               camera->Update(rotation);
          }
     }

}
