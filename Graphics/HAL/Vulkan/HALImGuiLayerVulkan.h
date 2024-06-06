#pragma once
#include "HAL/HALImGuiLayer.h"
#include "HAL/Vulkan/API/VulkanCmdContext.h"

#if defined(DEVELOP_BUBUG_TOOLS) && defined(ENABLE_IMGUI)
namespace Shard::HAL::Vulkan
{
    class HALImGuiLayerWrapperVulkan : public HALImGuiLayerWrapper
    {
    public:
        void Init() override;
        void UnInit() override;
        void NewFrameGameThread() override;
        void Render(HAL::HALCommandContext* cmd_buffer) override;
        ~HALImGuiLayerWrapperVulkan() { UnInit(); }
    private:
        void RenderImpl(ImDrawData* draw_data, VulkanCmdBuffer* cmd_buffer);
    private:
        ImGui_ImplVulkanH_Window    window_;
        VulkanFrameBuffer::SharedPtr fbo_;
        VulkanRenderPass::SharedPtr    pass_;
        VulkanQueue::SharedPtr    queue_;
    };

}
#endif

