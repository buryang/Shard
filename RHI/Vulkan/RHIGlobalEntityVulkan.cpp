#include "RHI/Vulkan/RHICommandVulkan.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"

namespace MetaInit::RHI::Vulkan {
	namespace
	{
		static RHIGlobalEntityVulkan global_entity;
		RegistGlobalEntity(RHIGlobalEntityVulkan, RHIBackEnd::eVulkan, [](void) { return &global_entity; });
	}

	void RHIGlobalEntityVulkan::Init(RHIFeatureLevel feature_level)
	{
	}

	RHICommandContext::Ptr RHIGlobalEntityVulkan::CreateCommandBuffer()
	{
		return static_cast<RHICommandContextVulkan*>(nullptr);
	}

}