#include "RHI/Vulkan/RHIResourcesVulkan.h"

namespace MetaInit::RHI::Vulkan
{
	void RHITextureVulkan::SetRHI(RHIGlobalEntity::Ptr rhi_entity)
	{
	}
	void RHITextureVulkan::Release(RHIGlobalEntity::Ptr rhi_entity)
	{
		ReleaseMemoryAllocation(memory_);
	}
	void RHIBufferVulkan::SetRHI(RHIGlobalEntity::Ptr rhi_entity)
	{
	}
	void RHIBufferVulkan::Release(RHIGlobalEntity::Ptr rhi_entity)
	{
		ReleaseMemoryAllocation(memory_);
	}
	void* RHIBufferVulkan::MapBackMem()
	{

	}
	void RHIBufferVulkan::UnMapBackMem()
	{
	}
}