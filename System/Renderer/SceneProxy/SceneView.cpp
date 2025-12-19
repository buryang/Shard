#include "../FXRModule/Base/FXRBaseModule.h"
#include "SceneView.h"

namespace Shard::Renderer
{
    SceneView::SceneView(const String& name, const ECSCameraComponent& camera) :name_(name), fov_(camera.fov_), near_clip(camera.near_clip_), far_clip_(camera.far_clip_)
    {
    }

    void SceneView::Update(ECSCameraComponent& camera, uint64_t frame_index)
    {
    }

}


