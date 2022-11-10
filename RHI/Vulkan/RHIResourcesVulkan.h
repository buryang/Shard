#pragma once

#include "RHI/RHIResources.h"
#include "RHI/Vulkan/API/VulkanResources.h"
#include "RHI/Vulkan/RHIResourceAllocatorVulkan.h"

namespace MetaInit::RHI::Vulkan
{
	class RHITextureVulkan final : public RHITexture
	{
	public:
		RHITextureVulkan(const RHITextureDesc& desc) :RHITexture(desc) {}
		void SetRHI(RHIGlobalEntity::Ptr rhi_entity) override;
		void Release(RHIGlobalEntity::Ptr rhi_entity) override;
		size_t GetOccupySize() const override;
		~RHITextureVulkan() { Release(); }
	private:
		VulkanImage	texture_;
		MemoryAllocation memory_;
	};

	class RHIBufferVulkan final : public RHIBuffer
	{
	public:
		RHIBufferVulkan(const RHIBufferDesc& desc) :RHIBuffer(desc) {}
		void SetRHI(RHIGlobalEntity::Ptr rhi_entity) override;
		void Release(RHIGlobalEntity::Ptr rhi_entity) override;
		size_t GetOccupySize() const override;
		void* MapBackMem() override;
		void UnMapBackMem() override;
		~RHIBufferVulkan() { Release(); }
	private:
		VulkanBuffer buffer_;
		MemoryAllocation memory_;
	};
}