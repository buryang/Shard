#pragma once
#include "RHI/RHIMemoryResidency.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"

namespace Shard::RHI::Vulkan
{
    class RHIMemoryResidencyManagerVulkan final : public RHIMemoryResidencyManager
    {
    public:
        RHIMemoryResidencyManagerVulkan(VulkanInstance::SharedPtr instance, VulkanDevice::SharedPtr device);
        RHIManagedMemoryCreateInfo GetResourceResidencyInfo(RHI::RHIResource::Ptr res_ptr) const override;
        bool IsManagedMemoryCreateInfoCompatible(const RHIManagedMemoryCreateInfo& lhs, const RHIManagedMemoryCreateInfo& rhs) const override;
        RHIManangedMemoryBlock* AllocManagedBlock(RHIMemoryTypeFlags type, RHIMemoryHeapFlags heap, uint64_t sz) override;
        RHIDedicatedMemoryBlock* AllocDedicatedBlock(const RHIManagedMemoryCreateInfo& mem_info) override;
        RHIManagedMemory* AllocManagedMemory(const RHIManagedMemoryCreateInfo& mem_info, RHIManangedMemoryBlock* parent = nullptr) override;
    protected:
        void GetMemoryBudget(RHIMemBudget& budget, RHIMemoryTypeFlags type, RHIMemoryHeapFlags heap) const override;
    private:
        VulkanInstance::SharedPtr instance_;
        VulkanDevice::SharedPtr device_;
    };
}
