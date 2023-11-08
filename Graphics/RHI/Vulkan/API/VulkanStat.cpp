#include "Utils/CommonUtils.h"
#include "RHI/Vulkan/API/VulkanStat.h"

#if defined(DEVELOP_DEBUG_TOOLS)&&defined(DEBUG_VULKAN)

VkBool32 DebugReportCallbackEXT(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, uint64_t object, size_t location,
                                int32_t message_code, const char* layer_prefix, const char* message, void* user_data)
{
    // If pMessage contains "specialuse-extension", then exit
    if (strstr(message, "specialuse-extension") != nullptr) {
        return VK_FALSE;
    };
}

VkBool32 DebugUtilsMessengerCallbackEXT(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types, 
                                            const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
    // If pMessageIdName contains "specialuse-extension", then exit
    if (strstr(callback_data->pMessageIdName, "specialuse-extension") != nullptr) {
        return VK_FALSE;
    };
}

#endif