#pragma once
#include "RHI/RHIImGuiLayer.h"

namespace MetaInit::RHI::Vulkan
{
	class RHIImGuiLayerWrapperVulkan : public RHIImGuiLayerWrapper
	{
	public:
		void Init() override;
		void UnInit() override;
		void NewFrameGameThread() override;
		void Render() override;
		~RHIImGuiLayerWrapperVulkan() { UnInit(); }
	private:
		void RenderImpl(ImDrawData* draw_data);
	private:
		ImGui_ImplVulkanH_Window	window_;
		VulkanFrameBuffer::SharedPtr fbo_;
		VulkanRenderPass::SharedPtr	pass_;
		VulkanQueue::SharedPtr	queue_;
	};

}

