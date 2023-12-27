#pragma once
#include "RHI/RHIImGuiLayer.h"
#include "RHI/Vulkan/API/VulkanCmdContext.h"

#if defined(DEVELOP_BUBUG_TOOLS) && defined(ENABLE_IMGUI)
namespace Shard::RHI::Vulkan
{
    class RHIImGuiLayerWrapperVulkan : public RHIImGuiLayerWrapper
    {
    public:
        void Init() override;
        void UnInit() override;
        void NewFrameGameThread() override;
        void Render(RHI::RHICommandContext::Ptr cmd_buffer) override;
        ~RHIImGuiLayerWrapperVulkan() { UnInit(); }
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

