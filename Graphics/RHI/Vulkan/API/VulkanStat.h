#pragma once
#include "RHI/Vulkan/API/VulkanRHI.h"

namespace Shard
{
    //https://docs.vulkan.org/guide/latest/validation_overview.html
#if defined(DEVELOP_DEBUG_TOOLS) && defined(DEBUG_VULKAN)
    VkBool32 DebugReportCallbackEXT(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, uint64_t object, size_t location,
        int32_t message_code, const char* layer_prefix, const char* message, void* user_data);
    VkBool32 DebugUtilsMessengerCallbackEXT(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data);
#endif
}