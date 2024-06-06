#pragma once
#include "Utils/Memory.h"
#include "Graphics/HAL/HALCommonUtils.h"
#include "Graphics/HAL/HALResources.h"
#include "Graphics/HAL/HALImGuiLayer.h"
#include "Graphics/HAL/HALShaderLibrary.h"
#include "Graphics/HAL/HALGlobalEntity.h"
#include "Graphics/HAL/HALMemoryResidency.h"
#include "Graphics/HAL/HALResourcePool.h"

namespace Shard::HAL {
    template <uint32_t backend>
    struct HALTypeConcreteTraits
    {
        using HALVertexShader = HALVertexShader;
        using HALHullShader = HALHullShader;
        using HALDomainSHader = HALDomainShader;
        using HALGeometryShader = HALGeometryShader;
        using HALPixelShader = HALPixelShader;
        using HALComputeShader = HALComputeShader;
        using HALPipelineStateObject = HALPipelineStateObject;
        using HALGlobalEntity = HALGlobalEntity;
        using HALShaderLibraryInterface = HALShaderLibraryInterface;
        using HALPipelineStateObjectLibraryInterface = HALPipelineStateObjectLibraryInterface;

        //resource related
        using HALTexture = HALTexture;
        using HALBuffer = HALBuffer;
        using HALSampler = HALSampler;
#if defined(DEVELOP_BUBUG_TOOLS) && defined(ENABLE_IMGUI)
        using HALImGuiLayerWrapper = HALImGuiLayerWrapper;
#endif
        using HALMemoryResidencyManager = HALMemoryResidencyManager;
    };

    //pre declare vulkan releate class here to avoid include too mush headers
    namespace Vulkan
    {
        class HALGlobalEntityVulkan;
        class HALVetexShaderVulkan;
        class HALHullShaderVulkan;
        class HALDomainShaderVulkan;
        class HALGeometryShaderVulkan;
        class HALPixelShaderVulkan;
        class HALComputeShaderVulkan;
        class HALPipelineStateObjectVulkan;
        class HALTextureVulkan;
        class HALBufferVulkan;
        class HALShaderLibraryVulkan;
        class HALPipelineStateObjectLibraryVulkan;
        class HALResourceBindlessSetVulkan;
        class HALImGuiLayerWrapperVulkan;
        class HALMemoryResidencyManagerVulkan;
    }

    template<>
    struct HALTypeConcreteTraits<EnumToInteger(EHALBackEnd::eVulkan)>
    {
        using HALVertexShader = Vulkan::HALVetexShaderVulkan;
        using HALHullShader = Vulkan::HALHullShaderVulkan;
        using HALDomainSHader = Vulkan::HALDomainShaderVulkan;
        using HALGeometryShader = Vulkan::HALGeometryShaderVulkan;
        using HALPixelShader = Vulkan::HALPixelShaderVulkan;
        using HALComputeShader = Vulkan::HALComputeShaderVulkan;
        using HALPipelineStateObject = Vulkan::HALPipelineStateObjectVulkan;

        using HALGlobalEntity = Vulkan::HALGlobalEntityVulkan;
        using HALShaderLibraryInterface = Vulkan::HALShaderLibraryVulkan;
        using HALPipelineStateObjectLibraryInterface = Vulkan::HALPipelineStateObjectLibraryVulkan;

        using HALTexture = Vulkan::HALTextureVulkan;
        using HALBuffer = Vulkan::HALBufferVulkan;
        using HALResourceBindlessHeap = Vulkan::HALResourceBindlessSetVulkan;
#if defined(DEVELOP_BUBUG_TOOLS) && defined(ENABLE_IMGUI)
        using HALImGuiLayerWrapper = Vulkan::HALImGuiLayerWrapperVulkan;
#endif
        using HALMemoryResidencyManager = Vulkan::HALMemoryResidencyManagerVulkan;
    };
}