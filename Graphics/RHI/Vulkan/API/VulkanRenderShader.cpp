#include "RHI/Vulkan/API/VulkanRenderShader.h"
#include <fstream>

namespace Shard {

    VkShaderModuleCreateInfo MakeShaderModuleCreateInfo(const Span<char>& code)
    {
        VkShaderModuleCreateInfo module_info{};
        memset(&module_info, 0, sizeof(module_info));
        module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        module_info.pCode = reinterpret_cast<const uint32_t*>(code.data());
        //codeSize is the size, in bytes, of the code pointed to by pCode
        module_info.codeSize = code.size();
        return module_info;
    }

    VulkanShaderModule::VulkanShaderModule(VulkanDevice::Ptr device, const Span<char>&code )
    {
        const auto& create_info = MakeShaderModuleCreateInfo(code);
        Init(device, create_info);
    }

    void VulkanShaderModule::Init(VulkanDevice::Ptr device, const VkShaderModuleCreateInfo& create_info)
    {
        assert(handle_ == VK_NULL_HANDLE);
        vkCreateShaderModule(device->Get(), &create_info, g_host_alloc, &handle_);
    }

    VulkanShaderModule::VulkanShaderModule(VulkanDevice::Ptr device, const VkShaderModuleCreateInfo& create_info):device_(device)
    {
        vkCreateShaderModule(device->Get(), &create_info, g_host_alloc, &handle_);
    }

    VulkanShaderModule::~VulkanShaderModule()
    {
        if (VK_NULL_HANDLE != handle_)
        {
            vkDestroyShaderModule(device_->Get(), handle_, g_host_alloc);
        }
    }
    VkShaderModule VulkanShaderModule::Get()
    {
        return handle_;
    }

    VulkanShaderModule::EType VulkanShaderModule::Type() const
    {
        return shader_type_;
    }
}