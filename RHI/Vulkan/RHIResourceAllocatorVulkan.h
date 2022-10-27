#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHIResources.h"
#include "RHI/Vulkan/API/VulkanRHI.h"

namespace MetaInit::RHI::Vulkan
{

	constexpr size_t RHI_PER_HEAP0_ALLOC_SIZE = 256;
	constexpr size_t RHI_PER_HEAP1_ALLOC_SIZE = 64; //total size limited on windows to 256MB
	constexpr size_t RHI_PER_HEAP2_ALLOC_SIZE = RHI_PER_HEAP0_ALLOC_SIZE;

	class RHIPooledResourceAllocatorVulkan
	{
	public:
		~RHIPooledResourceAllocatorVulkan();
	private:
		RHIResource::Ptr FindSuitAbleResource()const;
	private:
		List<RHIResource::Ptr> pooled_repo_;
		VulkanDevice::SharedPtr	device_{ nullptr };
	};

	class RHIHeapMemoryBulkVulkan
	{
	public:
		struct Range {
			RHIResource::Ptr	prev_resource_{ nullptr };
			uint32_t	offset_: 16{0};
			uint32_t	size_ : 16{0};
		};
	private:
		bool FindSuitAbleFreeRange(Range& range)const;
	private:
		List<Range>	free_blks_;
	};

	class RHITransientResourceAllocatorVulkan
	{
	public:
		~RHITransientResourceAllocatorVulkan();
	private:
		SmallVector<RHIHeapMemoryBulkVulkan>	mem_heaps_;
		VulkanDevice::SharedPtr	device_{ nullptr };
	};
}
