#pragma once
#include "Utils/CommonUtils.h"
#include "Core/EngineGlobalParams.h"
#include "RHI/RHIResources.h"
#include "RHI/RHICommand.h"
#include "RHI/RHIResourceBinding.h"
#include "RHI/RHIShaderLibrary.h"

namespace Shard::RHI
{
	enum class ERHIBackEnd : uint8_t
	{
		eNone,
		eVulkan, //now only support vulkan
		eD3D,
		eMetal,
	};

	static inline constexpr bool IsBackEndSupported(ERHIBackEnd back_end) {
		LOG(INFO) << "current only support vulkan backend";
		return back_end == ERHIBackEnd::eVulkan;
	}

	enum class ERHIFeatureLevel : uint8_t
	{
		eS3_1,
		eSM5,
		eSM6,
		eNum,
	};

	///global entity parameters
	REGIST_PARAM_TYPE(UINT, RHI_ENTITY_BACKEND, ERHIBackEnd::eVulkan);
	REGIST_PARAM_TYPE(UINT, RHI_FEATURE_LEVEL, ERHIFeatureLevel::eSM5);
	REGIST_PARAM_TYPE(BOOL, RHI_ASYNC_COMPUTE, false);

	//like unreal global dynamic RHI
	class MINIT_API RHIGlobalEntity
	{
	public:
		using Ptr = RHIGlobalEntity*;
		using CreateFunc = std::function<Ptr(void)>;
		static Ptr Instance();
		static bool RegistCreateFunc(ERHIBackEnd back_end, CreateFunc&& func);
		virtual ~RHIGlobalEntity() {}
		virtual void Init();
		virtual void UnInit();
		virtual void CreateSampler();
		virtual void CreateViewPoint();
		//only metal support shader library
		virtual RHIShaderLibraryInterface::Ptr GetOrCreateShaderLibrary();
		virtual RHIPipelineStateObjectLibraryInterface::Ptr GetOrCreatePSOLibrary();
		//bindless heap interface
		virtual RHIResourceBindlessHeap::SharedPtr GetResourceBindlessHeap();
#if defined(DEVELOP_DEBUG_TOOLS)&&defined(ENABLE_IMGUI)
		virtual RHIImGuiLayerWrapper::Ptr GetImGuiLayerWrapper() { return nullptr; }
#endif
		virtual RHIResource::Ptr CreateUniformBuffer(const RHIBufferInitializer& desc);
		virtual RHIResource::Ptr CreateStructedBuffer(const RHIBufferInitializer& desc);
		virtual RHITexture::Ptr CreateTexture(const RHITextureInitializer& desc);
		virtual RHIResource::Ptr CreateUAV(const RHITextureUAVInitializer& desc);
		virtual RHIResource::Ptr CreateSRV(const RHITextureSRVInitializer& desc);
		//virtual RHIResource::Ptr CreateRayTracingAccelerateStruct();
		virtual RHICommandContext::Ptr CreateCommandBuffer();
		//calculate resource video memory size
		virtual size_t ComputeMemorySize(RHIResource::Ptr res) const;
		virtual void SetViewPoint();
		virtual void ResizeViewPoint();
		virtual void Execute(Span<RHICommandContext::Ptr> cmd_buffers);

		//hardware property check
		virtual bool IsAsyncComputeSupported()const {
			LOG(ERROR) << "async compute check function not exsited";
		}
		virtual bool IsHWRayTraceSupported()const {
			LOG(ERROR) << "hardware ray tracing check function not exisited";
		}
	private:
		RHIGlobalEntity() = default;
		DISALLOW_COPY_AND_ASSIGN(RHIGlobalEntity);
	private:
		static Map<ERHIBackEnd, CreateFunc> create_func_repo_;
	};

//regist global entity create func at compile time
#define REGIST_GLOBAL_ENTITY(name, back_end, creat_func) \
static constexpr bool gxxx_##name##_created = RHIGlobalEntity::RegistCreateFunc(back_end, create_func);
}
