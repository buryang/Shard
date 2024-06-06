#pragma once
#include "imgui/imgui.h"
#include "Utils/CommonUtils.h"

#if defined(DEVELOP_BUBUG_TOOLS) && defined(ENABLE_IMGUI)
namespace Shard::HAL
{
    class MINIT_API HALImGuiLayerWrapper
    {
    public:
        using Ptr = HALImGuiLayerWrapper*;
        virtual void Init(){
            ImGui::CreateContext();
        }
        virtual void UnInit() {
            ImGui::DestroyContext();
        }
        virtual void NewFrameGameThread() = 0; //in master thread
        virtual void Render(HAL::HALCommandContext* cmd_buffer) = 0; //in render thread
        virtual ~HALImGuiLayerWrapper() = default;
    };
}
#endif
