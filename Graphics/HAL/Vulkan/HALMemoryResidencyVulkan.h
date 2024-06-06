#pragma once
#include "HAL/HALMemoryResidency.h"
#include "HAL/Vulkan/HALGlobalEntityVulkan.h"

namespace Shard::HAL::Vulkan
{
    /**
     * \brief realize vulkan gpu memory allocator, now do not support buffer and linear image
     * or optimal image allocated on one block, seperated by bufferimageganularity, but allocated
     * on different block like D3D iter1
     */
    enum {
        VULKAN_TILING_CATEGORY = VK_IMAGE_TILING_LINEAR + 1, //linear and optimal
    };

    class HALMemoryResidencyManagerVulkan final : public HALMemoryResidencyManager
    {
    public:
        void Init(HALGlobalEntity* rhi_entity) override;
        bool IsUMA() const override;
        bool IsMapInResourceLevel() const override { return false; }
        HALAllocationCreateInfo GetBufferResidencyInfo(HALBuffer* res_ptr) const override;
        HALAllocationCreateInfo GetTextureResidencyInfo(HALTexture* res_ptr) const override;
        void Evict(HALResource* res_ptr) override;
        void* Map(HALResource* res_ptr, HALAllocation* allocation) override;
        void UnMap(HALResource* res_ptr, HALAllocation* allocation) override;
        void GetAllocationBackendSpecInfoApprox(HALAllocationCreateInfo& create_info) override;
    protected:
        HALSizeType CalcPreferredBlockSizeHint(uint16_t pool_index)const;
        uint32_t GetMaxAllocationCount() const override;
        uint8_t FindMemoryPoolIndex(HALMemoryUsage usage, HALMemoryFlags flags, void* user_data) const override;
        void UpdateMemoryBudget() override;
        void MakeBufferResidentImpl(HALBuffer* res_ptr, void* rhi_mem, HALSizeType offset) override;
        void MakeTextureResidentImpl(HALTexture* res_ptr, void* rhi_mem, HALSizeType offset) override;
        void* MallocRawMemory(uint32_t pool_index, uint64_t size, float priority, void* plat_data = nullptr, void* user_data = nullptr) override;
        void FreeRawMemory(void* memory,uint32_t pool_index, HALSizeType offset, HALSizeType size, void*& mapped) override;
        void MapRawMemory(void* memory, HALSizeType offset, HALSizeType size, uint32_t flags, void*& mapped) override;
        void UnMapRawMemory(void* memory) override;
    private:
        void GetBufferMemoryRequirements(VkBuffer buffer, VkMemoryRequirements& requirements, HALMemoryFlags& flags)const;
        void GetImageMemoryRequirements(VkImage image, VkMemoryRequirements& requirements, HALMemoryFlags& flags)const;
        uint32_t GetPoolIndexFromTypeIndex(uint32_t type_index, bool is_optimal) const;
        uint32_t GetTypeIndexFromPoolIndex(uint32_t pool_index) const;
    private:
        VulkanInstance::SharedPtr instance_;
        VulkanDevice::SharedPtr device_;
        VkPhysicalDeviceMemoryProperties mem_props_;

    };
}
