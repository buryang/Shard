#include "Utils/CommonUtils.h"
#include "HAL/Vulkan/API/VulkanStat.h"

#if defined(DEVELOP_DEBUG_TOOLS)&&defined(DEBUG_VULKAN)

VkBool32 DebugReportCallbackEXT(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, uint64_t object, size_t location,
                                int32_t message_code, const char* layer_prefix, const char* message, void* user_data)
{
    // If message contains "specialuse-extension", then exit
    if (strstr(message, "specialuse-extension") != nullptr) {
        return VK_FALSE;
    };
    const auto fmt_msg = fmt::format("Vulkan Debug: Object ({}): msg_code: {}\n\t{}", object, message);
    switch (flags) {
    case VK_DEBUG_REPORT_DEBUG_BIT_EXT:
    case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:
        DLOG(INFO) << fmt_msg;
        break;
    case VK_DEBUG_REPORT_WARNING_BIT_EXT:
    case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT:
        LOG(WARNING) << fmt_msg;
        break;
    case VK_DEBUG_REPROT_ERROR_BIT_EXT:
        LOG(ERROR) << fmt_msg;
        break;
    default:
    }
    return VK_FALSE;
}

VkBool32 DebugUtilsMessengerCallbackEXT(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types, 
                                            const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
    // If pMessageIdName contains "specialuse-extension", then exit
    if (strstr(callback_data->pMessageIdName, "specialuse-extension") != nullptr) {
        return VK_FALSE;
    };
    if (strstr(callback_data->pMessage, "Mapping an image with layout") != nullptr && strstr(callback_data->pMessage, "can result in undefined behavior if this memory is used by the device") != nullptr) {
        return VK_FALSE;
    }
    // This needs to be ignored because Validator is wrong here.
    if (strstr(callback_data->pMessage, "Invalid SPIR-V binary version 1.3") != nullptr) {
        return VK_FALSE;
    }
    // This needs to be ignored because Validator is wrong here.
    if (strstr(callback_data->pMessage, "Shader requires flag") != nullptr) {
        return VK_FALSE;
    }

    // This needs to be ignored because Validator is wrong here.
    if (strstr(callback_data->pMessage, "SPIR-V module not valid: Pointer operand") != nullptr && strstr(callback_data->pMessage, "must be a memory object") != nullptr) {
        return VK_FALSE;
    }

    if (callback_data->pMessageIdName && strstr(callback_data->pMessageIdName, "UNASSIGNED-CoreValidation-DrawState-ClearCmdBeforeDraw") != nullptr) {
        return VK_FALSE;
    }

    std::ostringstream msg_buffer;
    auto msg_fmt = "({}/{}): msgNum:{}-{}\nObjects: {}\n";
    String type_string;
    switch (types)
    {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
        type_string = "GENERAL";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
        type_string = "VALIDATION";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        type_string = "PERFORMANCE";
        break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        type_string = "VALIDATION|PERFORMANCE";
        break;
    }
    msg_buffer << fmt::format(msg_fmt, severity, types, callback_data->messageIdNumber, callback_data->pMessage, callback_data->objectCount);
    for (auto n = 0u; n < callback_data->objectCount; ++n) {
        msg_buffer << fmt::format("[{}] {0:#x}, type:{0:d}, name:{}\n", n, callback_data->pObjects[n].objectHandle, callback_data->pObjects[n].objectType, callback_data->pObjects[n].pObjectName);
    }

    switch (severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        VLOG(1) << msg_buffer.str();
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        LOG(WARNING) << msg_buffer.str();
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        LOG(ERROR) << msg_buffer.str();
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
        break;
    }

    return false;
}

#endif