#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHIResource.h"
#include "RHI/RHICommand.h"

namespace MetaInit::RHI
{
	enum class RHIFeatureLevel :uint8_t
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
		static Ptr Instance();
		void Init(RHIFeatureLevel feature_level);
		void UnInit();
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
		//calculate resource video memory size
		virtual size_t ComputeMemorySize(RHIResource::Ptr res) const;
		virtual void SetViewPoint();
		virtual void ResizeViewPoint();
		virtual void Execute(Span<RHICommandContext::Ptr> cmd_buffers);
	private:
		RHIGlobalEntity() = default;
		DISALLOW_COPY_AND_ASSIGN(RHIGlobalEntity);
	};
}
