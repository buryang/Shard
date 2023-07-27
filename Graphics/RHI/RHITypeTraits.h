#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHIResources.h"
#include "RHI/RHIImGuiLayer.h"
#include "RHI/RHIShaderLibrary.h"
#include "RHI/RHIGlobalEntity.h"

namespace Shard::RHI {
	template <uint32_t backend>
	struct RHITypeConcreteTraits
	{
		using RHIVertexShader = RHIVertexShader;
		using RHIHullShader = RHIHullShader;
		using RHIDomainSHader = RHIDomainShader;
		using RHIGeometryShader = RHIGeometryShader;
		using RHIPixelShader = RHIPixelShader;
		using RHIComputeShader = RHIComputeShader;
		using RHIPipelineStateObject = RHIPipelineStateObject;
		using RHIGlobalEntity = RHIGlobalEntity;
		using RHIShaderLibraryInterface = RHIShaderLibraryInterface;
		using RHIPipelineStateObjectLibraryInterface = RHIPipelineStateObjectLibraryInterface;

		//resource related
		using RHITexture = RHITexture;
		using RHIBuffer = RHIBuffer;
		using RHISampler = RHISampler;
		using RHIImGuiLayerWrapper = RHIImGuiLayerWrapper;
	};

	//pre declare vulkan releate class here to avoid include too mush headers
	namespace Vulkan
	{
		class RHIGlobalEntityVulkan;
		class RHIVetexShaderVulkan;
		class RHIHullShaderVulkan;
		class RHIDomainShaderVulkan;
		class RHIGeometryShaderVulkan;
		class RHIPixelShaderVulkan;
		class RHIComputeShaderVulkan;
		class RHIPipelineStateObjectVulkan;
		class RHITextureVulkan;
		class RHIBufferVulkan;
		class RHIShaderLibraryVulkan;
		class RHIPipelineStateObjectLibraryVulkan;
		class RHIResourceBindlessSetVulkan;
		class RHIImGuiLayerWrapperVulkan;
	}

	template<>
	struct RHITypeConcreteTraits<EnumToInteger(ERHIBackEnd::eVulkan)>
	{
		using RHIVertexShader = Vulkan::RHIVetexShaderVulkan;
		using RHIHullShader = Vulkan::RHIHullShaderVulkan;
		using RHIDomainSHader = Vulkan::RHIDomainShaderVulkan;
		using RHIGeometryShader = Vulkan::RHIGeometryShaderVulkan;
		using RHIPixelShader = Vulkan::RHIPixelShaderVulkan;
		using RHIComputeShader = Vulkan::RHIComputeShaderVulkan;
		using RHIPipelineStateObject = Vulkan::RHIPipelineStateObjectVulkan;

		using RHIGlobalEntity = Vulkan::RHIGlobalEntityVulkan;
		using RHIShaderLibraryInterface = Vulkan::RHIShaderLibraryVulkan;
		using RHIPipelineStateObjectLibraryInterface = Vulkan::RHIPipelineStateObjectLibraryVulkan;

		using RHITexture = Vulkan::RHITextureVulkan;
		using RHIBuffer = Vulkan::RHIBufferVulkan;
		using RHIResourceBindlessHeap = Vulkan::RHIResourceBindlessSetVulkan;

		using RHIImGuiLayerWrapper = RHIImGuiLayerWrapperVulkan;
	};
}