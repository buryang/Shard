#pragma once
#include "Utils/CommonUtils.h"
#include "Core/RenderGlobalParams.h"
#include "RHI/RHIResources.h"
#include "RHI/RHICommand.h"

namespace MetaInit::RHI
{
	enum class ERHIBackEnd : uint8_t
	{
		eNone,
		eVulkan, //now only support vulkan
	};

	static inline constexpr bool IsBackEndSupported(ERHIBackEnd back_end) {
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
		virtual void CreateGFXPipelineState();
		virtual void CreateComputePipelineState();
		virtual void CreateSampler();
		virtual void CreateViewPoint();
		virtual RHIResource::Ptr CreateConstBuffer(const RHIBufferDesc& desc);
		virtual RHIResource::Ptr CreateStructedBuffer(const RHIBufferDesc& desc);
		virtual RHIResource::Ptr CreateTexture(const RHITextureDesc& desc);
		virtual RHIResource::Ptr CreateUAV(const RHITextureUAVDesc& desc);
		virtual RHIResource::Ptr CreateSRV(const RHITextureSRVDesc& desc);
		//virtual RHIResource::Ptr CreateRayTracingAccelerateStruct();
		virtual RHICommandContext::Ptr CreateCommandBuffer();
		//calculate resource video memory size
		virtual size_t ComputeMemorySize(RHIResource::Ptr res) const;
		virtual void SetViewPoint();
		virtual void ResizeViewPoint();
		virtual void Execute(Span<RHICommandContext::Ptr> cmd_buffers);
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
