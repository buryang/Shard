#pragma once
#include "RHI/RHIMemoryResidency.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"

namespace Shard::RHI::Vulkan
{
    class RHIMemoryResidencyManagerVulkan final : public RHIMemoryResidencyManager
    {
    public:
        RHIMemoryResidencyManagerVulkan(VulkanInstance::SharedPtr instance, VulkanDevice::SharedPtr device);
        void Init(RHIGlobalEntity::Ptr rhi_entity) override;
        bool IsUMA() const override;
        RHIManagedMemoryCreateInfo GetResourceResidencyInfo(RHIResource::Ptr res_ptr) const override;
        void MakeResident(RHIResource::Ptr res_ptr, RHIManagedMemory& managed_mem, RHISizeType offset) override;
        void MakeResident(RHIResource::Ptr res_ptr, RHIDedicatedMemoryBlock& dedicated_mem) override;
        void Evict(RHIResource::Ptr res_ptr) override;
    protected:
        uint32_t CalcPreferredBlockSizeHint(uint16_t heap_index)const;
        uint8_t FindMemoryTypeIndex(RHIMemoryUsage usage, RHIMemoryFlags flags) const override;
        void* MallocRawMemory(uint32_t flags, uint64_t size, void* plat_data = nullptr, void* user_data = nullptr) override;
        void UpdateBudget();
    private:
        VulkanInstance::SharedPtr instance_;
        VulkanDevice::SharedPtr device_;
        VkPhysicalDeviceMemoryProperties2 mem_props_;
        
        VkPhysicalDeviceMemoryBudgetPropertiesEXT mem_budget_ext_;
    };
}
