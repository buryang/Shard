#pragma once

#include "RHI/RHIResources.h"
#include "RHI/Vulkan/API/VulkanResources.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"

namespace Shard::RHI::Vulkan
{
	class RHITextureVulkan final : public RHITexture
	{
	public:
		using Ptr = RHITextureVulkan*;
		using SharedPtr = eastl::shared_ptr<RHITextureVulkan>;
		RHITextureVulkan(RHIGlobalEntityVulkan::Ptr parent, const RHITextureInitializer& desc) :RHITexture(parent, desc) {}
		void operator=(RHITextureVulkan&& rhs);
		void SetUp() override;
		void Release() override;
		size_t GetOccupySize() const override;
		//only inital vulkan texture handle
		void SetUpHandleAlone();
		void SetUpHandleMemory(const MemoryAllocation& memory);
		FORCE_INLINE VulkanImage::SharedPtr GetImpl() {
			return texture_;
		}
		~RHITextureVulkan() { Release(); }
	private:
		friend class RHIGlobalEntityVulkan;
		VulkanImage::SharedPtr	texture_;
		MemoryAllocation memory_;
	};

	class RHIBufferVulkan final : public RHIBuffer
	{
	public:
		using Ptr = RHIBufferVulkan*;
		using SharedPtr = eastl::shared_ptr<RHIBufferVulkan>;
		RHIBufferVulkan(RHIGlobalEntityVulkan::Ptr parent, const RHIBufferInitializer& desc) :RHIBuffer(parent, desc) {}
		void operator=(RHIBufferVulkan&& rhs);
		void SetUp() override;
		void Release() override;
		size_t GetOccupySize() const override;
		void* MapBackMem() override;
		void UnMapBackMem() override;
		//only initial vulkan buffer handle
		void SetUpHandleAlone();
		void SetUpHandleMemory(const MemoryAllocation& memory);
		FORCE_INLINE VulkanBuffer::SharedPtr GetImpl() {
			return buffer_;
		}
		~RHIBufferVulkan() { Release(); }
	private:
		friend class RHIGlobalEntityVulkan;
		VulkanBuffer::SharedPtr buffer_;
		MemoryAllocation memory_;
	};
}