#pragma once
#include "imgui/imgui.h"
#include "Utils/CommonUtils.h"

#if defined(DEVELOP_BUBUG_TOOLS) && defined(ENABLE_IMGUI)
namespace Shard::RHI
{
        class MINIT_API RHIImGuiLayerWrapper
        {
        public:
                using Ptr = RHIImGuiLayerWrapper*;
                virtual void Init(){
                        ImGui::CreateContext();
                }
                virtual void UnInit() {
                        ImGui::DestroyContext();
                }
                virtual void NewFrameGameThread() = 0; //in master thread
                virtual void Render(RHI::RHICommandContext::Ptr cmd_buffer) = 0; //in render thread
                virtual ~RHIImGuiLayerWrapper() = default;
        };
}
#endif
