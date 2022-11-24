#pragma once

#include "RHI/RHIResources.h"
#include "RHI/Vulkan/API/VulkanResources.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"

namespace MetaInit::RHI::Vulkan
{
	class RHITextureVulkan final : public RHITexture
	{
	public:
		RHITextureVulkan(RHIGlobalEntityVulkan::Ptr parent, const RHITextureDesc& desc) :RHITexture(parent, desc) {}
		void operator=(RHITextureVulkan&& rhs);
		void SetUp() override;
		void Release() override;
		size_t GetOccupySize() const override;
		FORCE_INLINE VulkanImage::SharedPtr GetImpl() {
			return texture_;
		}
		~RHITextureVulkan() { Release(); }
	private:
		VulkanImage::SharedPtr	texture_;
		MemoryAllocation memory_;
	};

	class RHIBufferVulkan final : public RHIBuffer
	{
	public:
		RHIBufferVulkan(RHIGlobalEntityVulkan::Ptr parent, const RHIBufferDesc& desc) :RHIBuffer(parent, desc) {}
		void operator=(RHIBufferVulkan&& rhs);
		void SetUp() override;
		void Release() override;
		size_t GetOccupySize() const override;
		void* MapBackMem() override;
		void UnMapBackMem() override;
		FORCE_INLINE VulkanBuffer::SharedPtr GetImpl() {
			return buffer_;
		}
		~RHIBufferVulkan() { Release(); }
	private:
		VulkanBuffer::SharedPtr buffer_;
		MemoryAllocation memory_;
	};
}