#include "RHI/Vulkan/RHICommandVulkan.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"

namespace MetaInit::RHI::Vulkan {
	namespace
	{
		static RHIGlobalEntityVulkan global_entity;
		REGIST_GLOBAL_ENTITY(RHIGlobalEntityVulkan, RHIBackEnd::eVulkan, [](void)->auto { return &global_entity; });
	}

	void RHIGlobalEntityVulkan::Init(RHIFeatureLevel feature_level)
	{
	}

	RHICommandContext::Ptr RHIGlobalEntityVulkan::CreateCommandBuffer()
	{
		return static_cast<RHICommandContextVulkan*>(nullptr);
	}

}