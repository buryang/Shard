#pragma once
#include "Utils/CommonUtils.h"

#include "RHI/RHIResource.h"
#include "RHI/RHICommand.h"

namespace MetaInit::RHI
{

	class RHIQueue
	{
	public:
		using Ptr = RHIQueue*;
	};

	class RHIDevice
	{
	public:
		using Ptr = RHIDevice*;
	};

	class RHIInstance
	{
	public:
		using Ptr = RHIInstance*;
	};

	enum class RHIFeatureLevel :uint8_t
	{
		eS3_1,
		eSM5,
		eSM6,
		eNum,
	};

	class MINIT_API RHIGlobalEntity
	{
	public:
		static RHIGlobalEntity& Instance() {
			static RHIGlobalEntity rhi;
			return rhi;
		}
		void Init(RHIFeatureLevel feature_level);
		void UnInit();
		void CreateGFXPipelineState();
		void CreateComputePipelineState();
		RHIResource::Ptr CreateConstBuffer();
		RHIResource::Ptr CreateStructedBuffer();
		RHIResource::Ptr CreateTexture();
		RHIResource::Ptr CreateUAV();
		RHIResource::Ptr CreateSRV();
		RHIResource::Ptr CreateRayTracingAccelerateStruct();
		//calculate resource video memory size
		size_t ComputeMemorySize(RHIResource::Ptr res) const;
		void CreateSampler();
		void CreateViewPoint();
		void SetViewPoint();
		void ResizeViewPoint();

		RHIDevice::Ptr GetDevice();
		RHIInstance::Ptr GetInstance();
		RHICommandContext::Ptr GetGFXCommandContext();
		RHICommandContext::Ptr GetComputeCommandContext(bool is_async = false);
		void ExecuteCommands(const RHICommandContext* commands, uint32_t count);
	private:
		virtual void* QueryDevice() = 0;
	private:
		RHIGlobalEntity() = default;
		DISALLOW_COPY_AND_ASSIGN(RHIGlobalEntity);
	};
}
