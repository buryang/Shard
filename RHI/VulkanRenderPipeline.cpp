#include "VulkanRenderPipeline.h"

#include "Utils/CommonUtils.h"
#include "VulkanCmdContext.h"


namespace MetaInit
{

	VkPipelineCacheCreateInfo MakePipelineCreateInfo(const Vector<VkDescriptorSetLayout>& ds_layout,
													const Vector<VkPushConstantRange>& push_range,
													VkPipelineCreateFlags flags)
	{

	}

	namespace
	{
		const VkPipelineBindPoint bind_point[] = {
			VK_PIPELINE_BIND_POINT_COMPUTE,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR
		};
	}

	void VulkanRenderPipeline::Bind(VulkanCmdBuffer& cmd_buffer)
	{
		vkCmdBindPipeline(cmd_buffer.Get(), bind_point[static_cast<uint8_t>(pipe_type_)], handle_);
	}
}