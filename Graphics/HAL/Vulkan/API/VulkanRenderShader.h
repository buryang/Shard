#pragma once
#include "Utils/CommonUtils.h"
#include "HAL/Vulkan/API/VulkanHAL.h"

namespace Shard
{
    class VulkanShaderModule;
    VkShaderModuleCreateInfo MakeShaderModuleCreateInfo(const Span<char>& code);

    struct VulkanResourceDiscriptors
    {
        std::vector<VkDescriptorSetLayout>    layout_;
        std::vector<int>                    dummy_;
    };

    class VulkanShaderModule
    {
    public:
        enum class EType
        {
            eVertex,
            ePixel,
            eCompute,
            eHull, //Tessellation control shader for vulkan
            //eTessellator,//fixed-function tessellator 
            eDomain,//tessellation evaluation shader for vulkan
            eGeometry,
            //ray tracing part
            eRaygen,
            eAnyHit,
            eMiss,
            eIntersction,
            eMesh, //Mesh shader 
            eTask,
            eCallable,
        };
        using Ptr = std::shared_ptr<VulkanShaderModule>;
        VulkanShaderModule() = default;
        explicit VulkanShaderModule(VulkanDevice* device, const VkShaderModuleCreateInfo& create_info);
        explicit VulkanShaderModule(VulkanDevice* device, const Span<char>& code);
        void Init(VulkanDevice* device, const VkShaderModuleCreateInfo& create_info);
        DISALLOW_COPY_AND_ASSIGN(VulkanShaderModule);
        ~VulkanShaderModule();
        VkShaderModule Get();
        EType Type()const;
    private:
        VkShaderModule    handle_{ VK_NULL_HANDLE };
        VulkanDevice*    device_;
        EType    shader_type_;
    };
}