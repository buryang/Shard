#pragma once
#include "RHI/RHIMemoryResidency.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"

namespace Shard::RHI::Vulkan
{
    /**
     * \brief realize vulkan gpu memory allocator, now do not support buffer and linear image
     * or optimal image allocated on one block, seperated by bufferimageganularity, but allocated
     * on different block like D3D iter1
     */
    enum {
        VULKAN_TILING_CATEGORY = VK_IMAGE_TILING_LINEAR + 1, //linear and optimal
    };

    class RHIMemoryResidencyManagerVulkan final : public RHIMemoryResidencyManager
    {
    public:
        void Init(RHIGlobalEntity::Ptr rhi_entity) override;
        bool IsUMA() const override;
        bool IsMapInResourceLevel() const override { return false; }
        RHIAllocationCreateInfo GetResourceResidencyInfo(RHIResource::Ptr res_ptr) const override;
        void MakeResident(RHIResource::Ptr res_ptr, RHIAllocation& allocation) override;
        void MakeResident(RHIResource::Ptr res_ptr, RHIManagedMemory& managed_mem, RHISizeType offset) override;
        void MakeResident(RHIResource::Ptr res_ptr, RHIDedicatedMemoryBlock& dedicated_mem) override;
        void Evict(RHIResource::Ptr res_ptr) override;
        void* Map(RHIResource::Ptr res_ptr, RHIAllocation* allocation) override;
        void UnMap(RHIResource::Ptr res_ptr, RHIAllocation* allocation) override;
        void GetAllocationBackendSpecInfoApprox(RHIAllocationCreateInfo& create_info) override;
    protected:
        RHISizeType CalcPreferredBlockSizeHint(uint16_t pool_index)const;
        uint32_t GetMaxAllocationCount() const override;
        uint8_t FindMemoryPoolIndex(RHIMemoryUsage usage, RHIMemoryFlags flags, void* user_data) const override;
        void UpdateMemoryBudget() override;
        void* MallocRawMemory(uint32_t pool_index, uint64_t size, float priority, void* plat_data = nullptr, void* user_data = nullptr) override;
        void FreeRawMemory(void* memory,RHISizeType offset, RHISizeType size, void*& mapped) override;
        void MapRawMemory(void* memory, RHISizeType offset, RHISizeType size, uint32_t flags, void*& mapped) override;
        void UnMapRawMemory(void* memory) override;
    private:
        void GetBufferMemoryRequirements(VkBuffer buffer, VkMemoryRequirements& requirements, RHIMemoryFlags& flags);
        void GetImageMemoryRequirements(VkImage image, VkMemoryRequirements& requirements, RHIMemoryFlags& flags);
        uint32_t GetPoolIndexFromTypeIndex(uint32_t type_index, bool is_optimal) const;
        uint32_t GetTypeIndexFromPoolIndex(uint32_t pool_index) const;
    private:
        VulkanInstance::SharedPtr instance_;
        VulkanDevice::SharedPtr device_;
        VkPhysicalDeviceMemoryProperties mem_props_;
        
        VkPhysicalDeviceMemoryBudgetPropertiesEXT mem_budget_ext_;
    };
}
