#pragma once
#include "RHI/RHIMemoryResidency.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"

namespace Shard::RHI::Vulkan
{
    class RHIMemoryResidencyManagerVulkan final : public RHIMemoryResidencyManager
    {
    public:
        void Init(RHIGlobalEntity::Ptr rhi_entity) override;
        bool IsUMA() const override;
        bool IsMapInResourceLevel() const override { return false; }
        RHIAllocationCreateInfo GetResourceResidencyInfo(RHIResource::Ptr res_ptr) const override;
        void MakeResident(RHIResource::Ptr res_ptr, RHIAllocation& allocation, RHISizeType offset) override;
        void MakeResident(RHIResource::Ptr res_ptr, RHIManagedMemory& managed_mem, RHISizeType offset) override;
        void MakeResident(RHIResource::Ptr res_ptr, RHIDedicatedMemoryBlock& dedicated_mem) override;
        void Evict(RHIResource::Ptr res_ptr) override;
        void* Map(RHIResource::Ptr res_ptr) override;
        void UnMap(RHIResource::Ptr res_ptr) override;
        void GetAllocationBackendSpecInfoApprox(RHIAllocationCreateInfo& create_info) override;
    protected:
        RHISizeType CalcPreferredBlockSizeHint(uint16_t heap_index)const;
        uint8_t FindMemoryTypeIndex(RHIMemoryUsage usage, RHIMemoryFlags flags) const override;
        void UpdateMemoryBudget() override;
        void* MallocRawMemory(uint32_t flags, uint64_t size, void* plat_data = nullptr, void* user_data = nullptr) override;
        void FreeRawMemory(void* memory) override;
        void MapRawMemory(void* memory, RHISizeType offset, RHISizeType size, uint32_t flags, void*& mapped) override;
        void UnMapRawMemory(void* memory) override;
    private:
        void GetBufferMemoryRequirements(VkBuffer buffer, VkMemoryRequirements& requirements, RHIMemoryFlags& flags);
        void GetImageMemoryRequirements(VkImage image, VkMemoryRequirements& requirements, RHIMemoryFlags& flags);
    private:
        VulkanInstance::SharedPtr instance_;
        VulkanDevice::SharedPtr device_;
        VkPhysicalDeviceMemoryProperties2 mem_props_;
        
        VkPhysicalDeviceMemoryBudgetPropertiesEXT mem_budget_ext_;
    };
}
