#include "RHI/Vulkan/RHIImGuiLayerVulkan.h"
#include "RHI/Vulkan/API/VulkanRenderPass.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"
#include "RHI/Vulkan/RHICommandVulkan.h"
#include "imgui/backends/imgui_impl_vulkan.h"

//https://github.com/ocornut/imgui/blob/master/examples/example_glfw_vulkan/main.cpp

namespace Shard::RHI::Vulkan
{
	void RHIImGuiLayerWrapperVulkan::Init()
	{
		RHIImGuiLayerWrapper::Init();
		auto device = RHIGlobalEntityVulkan::Instance();
		VulkanRenderPass::Desc pass_desc{};
		pass_ = VulkanRenderPass::Create(device->GetVulkanDevice(), pass_desc);
		queue_ = VulkanQueue::Create(VulkanQueue::EQueueType::eGraphics); //todo
		ImGui_ImplVulkan_InitInfo init_info{};
		init_info.Instance = device->GetVulkanInstance()->Get();
		init_info.PhysicalDevice = device->GetVulkanDevice()->GetPhyHandle();
		init_info.Device = device->GetVulkanDevice()->Get();
		init_info.DescriptorPool = nullptr;
		init_info.Allocator = g_host_alloc;
		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.Subpass = 0u;
		init_info.MinImageCount = 2;//todo
		init_info.ImageCount = 2; //todo
		init_info.Queue = queue_->Get();
		init_info.QueueFamily = queue_->GetFamilyIndex();
		ImGui_ImplVulkan_Init(&init_info, pass_->Get());
		//todo init pso
	}

	void RHIImGuiLayerWrapperVulkan::UnInit()
	{
		ImGui_ImplVulkan_Shutdown();
		RHIImGuiLayerWrapper::UnInit();
	}
	void RHIImGuiLayerWrapperVulkan::NewFrameGameThread()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui::NewFrame();
	}
	void RHIImGuiLayerWrapperVulkan::Render()
	{
		ImGui::Render();
		auto* draw_data = ImGui::GetDrawData();
		assert(draw_data != nullptr);
		const auto is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
		if (!draw_data->TotalVtxCount || !is_minimized) {
			RenderImpl(draw_data);
		}
	}
	
	void RHIImGuiLayerWrapperVulkan::RenderImpl(ImDrawData* draw_data)
	{
		auto* device = RHIGlobalEntityVulkan::Instance();
		auto cmd_buffer = dynamic_cast<RHICommandContextVulkan*>(device->CreateCommandBuffer());
		//gui_pass->Begin(cmd_buffer, frame_buffer);
		ImGui_ImplVulkan_RenderDrawData(draw_data, cmd_buffer->Get()->Get());
		//gui_pass->End(cmd_buffer);
	}
}

