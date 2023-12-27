#pragma once
#include "RHI/RHIShader.h"
#include "RHI/Vulkan/API/VulkanRenderShader.h"
#include "RHI/Vulkan/API/VulkanRenderPipeline.h"

namespace Shard::RHI::Vulkan {
    
    //think about whether inherit from both top rhi level and bottom level good 
    class RHIComputeShaderVulkan : public RHIComputeShader, public VulkanShaderModule {
    public:
        RHIComputeShaderVulkan(const RHIShaderInitializer& initializer);
    };

    class RHIVertexShaderVulkan : public RHIVertexShader, public VulkanShaderModule {
    public:
        RHIVertexShaderVulkan(const RHIShaderInitializer& initializer);
    };

    class RHIHullShaderVulkan : public RHIHullShader, public VulkanShaderModule {
    public:
        RHIHullShaderVulkan(const RHIShaderInitializer& initializer);
    };

    class RHIDomainShaderVulkan : public RHIDomainShader, public VulkanShaderModule {
    public:
        RHIDomainShaderVulkan(const RHIShaderInitializer& initializer);
    };

    class RHIGeometryShaderVulkan : public RHIGeometryShader, public VulkanShaderModule {
    public:
        RHIGeometryShaderVulkan(const RHIShaderInitializer& initializer);
    };

    class RHIPixelShaderVulkan : public RHIPixelShader, public VulkanShaderModule {
    public:
        RHIPixelShaderVulkan(const RHIShaderInitializer& initializer);
    };

    class RHIPipelineStateObjectVulkan : public RHIPipelineStateObject
    {
    public:
        using Ptr = RHIPipelineStateObjectVulkan*;
        RHIPipelineStateObjectVulkan(const RHIPipelineStateObjectInitializer& initializer);
        ~RHIPipelineStateObjectVulkan() {
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
        VulkanRenderPipeline::Ptr    pso_obj_{ nullptr };
    };
}
