#pragma once
#include "HAL/HALShader.h"
#include "HAL/Vulkan/API/VulkanRenderShader.h"
#include "HAL/Vulkan/API/VulkanRenderPipeline.h"

namespace Shard::HAL::Vulkan {
    
    //think about whether inherit from both top rhi level and bottom level good 
    class HALComputeShaderVulkan : public HALComputeShader, public VulkanShaderModule {
    public:
        HALComputeShaderVulkan(const HALShaderInitializer& initializer);
    };

    class HALVertexShaderVulkan : public HALVertexShader, public VulkanShaderModule {
    public:
        HALVertexShaderVulkan(const HALShaderInitializer& initializer);
    };

    class HALHullShaderVulkan : public HALHullShader, public VulkanShaderModule {
    public:
        HALHullShaderVulkan(const HALShaderInitializer& initializer);
    };

    class HALDomainShaderVulkan : public HALDomainShader, public VulkanShaderModule {
    public:
        HALDomainShaderVulkan(const HALShaderInitializer& initializer);
    };

    class HALGeometryShaderVulkan : public HALGeometryShader, public VulkanShaderModule {
    public:
        HALGeometryShaderVulkan(const HALShaderInitializer& initializer);
    };

    class HALPixelShaderVulkan : public HALPixelShader, public VulkanShaderModule {
    public:
        HALPixelShaderVulkan(const HALShaderInitializer& initializer);
    };

    class HALPipelineStateObjectVulkan : public HALPipelineStateObject
    {
    public:
        using Ptr = HALPipelineStateObjectVulkan*;
        HALPipelineStateObjectVulkan(const HALPipelineStateObjectInitializer& initializer);
        ~HALPipelineStateObjectVulkan() {
            if (pso_obj_ != nullptr) {
                delete pso_obj_;
            }
        }
        template <class ConcreteType>
        requires std::is_base_of_v<VulkanRenderPipeline, ConcreteType>
        ConcreteType* Get() {
            return dynamic_cast<ConcreteType*>(pso_obj_);
        }
    private:
        VulkanRenderPipeline*    pso_obj_{ nullptr };
    };
}
