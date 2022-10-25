#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHIResource.h"
#include "RHI/RHICommand.h"

namespace MetaInit::RHI
{
	enum class RHIBackEnd : uint8_t
	{
		eNone,
		eVulkan, //now only support vulkan
	};

	static inline bool IsBackEndSupported(RHIBackEnd back_end) {
		return back_end == RHIBackEnd::eVulkan;
	}

	enum class RHIFeatureLevel : uint8_t
	{
		eS3_1,
		eSM5,
		eSM6,
		eNum,
	};

	//like unreal global dynamic RHI
	class MINIT_API RHIGlobalEntity
	{
	public:
		using Ptr = RHIGlobalEntity*;
		using CreateFunc = std::function<Ptr(void)>;
		static Ptr Instance(RHIBackEnd back_end);
		static bool RegistCreateFunc(RHIBackEnd back_end, CreateFunc&& func);
		virtual ~RHIGlobalEntity() {}
		virtual void Init(RHIFeatureLevel feature_level);
		virtual void UnInit();
		virtual void CreateGFXPipelineState();
		virtual void CreateComputePipelineState();
		virtual void CreateSampler();
		virtual void CreateViewPoint();
		virtual RHIResource::Ptr CreateConstBuffer();
		virtual RHIResource::Ptr CreateStructedBuffer();
		virtual RHIResource::Ptr CreateTexture();
		virtual RHIResource::Ptr CreateUAV();
		virtual RHIResource::Ptr CreateSRV();
		virtual RHIResource::Ptr CreateRayTracingAccelerateStruct();
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
		static Map<RHIBackEnd, CreateFunc> create_func_repo_;
	};

#define RegistGlobalEntity(name, back_end, creat_func) \
static constexpr bool xxx_##name##_created = RHIGlobalEntity::RegistCreateFunc(back_end, create_func);
}
