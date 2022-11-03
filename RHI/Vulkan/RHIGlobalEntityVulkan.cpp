#include "RHI/Vulkan/RHICommandVulkan.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"

#define ADD_EXT_IF(CONDITION, EXT_NAME) if (CONDITION) { extensions.emplace_back(EXT_NAME); }
#define ADD_LAYER_IF(CONDITION, LAYER_NAME) if (CONDITION) { layers.emplace_back(LAYER_NAME); }

namespace MetaInit::RHI::Vulkan {
	
	REGIST_GLOBAL_ENTITY(RHIGlobalEntityVulkan, ERHIBackEnd::eVulkan, [](void){ return static_cast<RHIGlobalEntity::Ptr>(RHIGlobalEntityVulkan::Instance()); });

	//instance and device extension config
	REGIST_PARAM_TYPE(BOOL, INSTANCE_DEBUG_REPROT_EXT, true);
	REGIST_PARAM_TYPE(BOOL, INSTANCE_DEBUG_UTILS_EXT,  true);
	REGIST_PARAM_TYPE(BOOL, DEVICE_MEMORY_BUDGET, true);
	REGIST_PARAM_TYPE(BOOL, DEVICE_MEMORY_REQUIRE, true);
	REGIST_PARAM_TYPE(BOOL, DEVICE_DEDICATED_ALLOC, true);

	RHIGlobalEntityVulkan::Ptr RHIGlobalEntityVulkan::Instance()
	{
		static RHIGlobalEntityVulkan global_entity;
		return &global_entity;
	}

	void RHIGlobalEntityVulkan::Init()
	{
		instance_ = VulkanInstance::Create();
		device_ = VulkanDevice::Create(instance_);
	}

	RHICommandContext::Ptr RHIGlobalEntityVulkan::CreateCommandBuffer()
	{
		return static_cast<RHICommandContextVulkan*>(nullptr);
	}

	void RHIGlobalEntityVulkan::Execute(Span<RHICommandContext::Ptr> cmd_buffers)
	{
		SmallVector<RHICommandContextVulkan::Ptr> gfx_cmds;
		SmallVector<RHICommandContextVulkan::Ptr> compute_cmds;
		for (auto& cmd : cmd_buffers) {

		}
		if (gfx_cmds.size()) {
			auto gfx_queue = GetVulkanDevice()->GetQueue();
			gfx_queue->Submit(gfx_cmds);
		}
		if (compute_cmds.size()) {
			auto compute_queue = GetVulkanDevice()->GetQueue();
			compute_queue->Submit(compute_cmds);
		}
	}

	RHIGlobalEntityVulkan::RHIGlobalEntityVulkan() {

	}

	void RHIGlobalEntityVulkan::InitInstance()
	{
	}

}