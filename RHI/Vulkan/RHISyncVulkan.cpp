#include "RHISyncVulkan.h"

namespace Shard::RHI::Vulkan {
	RHIEventVulkan::RHIEventVulkan(const RHIEventInitializer& initializer):RHIEvent(initializer)
	{
		VkEventCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
			.pNext = nullptr,
			.flags = initializer.flags_
		};
		auto ret = vkCreateEvent(GetGlobalDevice(), &create_info, g_host_alloc, &handle_);
		PCHECK(ret == VK_SUCCESS) << "failed to create vulkan event";
	}

	RHIEventVulkan::~RHIEventVulkan()
	{
		if (handle_ != VK_NULL_HANDLE) {
			vkDestroyEvent(GetGlobalDevice(), handle_, g_host_alloc);
		}
	}
}
