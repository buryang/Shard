#include "RHIMemoryResidencyVulkan.h"

namespace Shard::RHI::Vulkan
{
    void RHIMemoryResidencyManagerVulkan::Init(RHIGlobalEntity::Ptr rhi_entity)
    {
        UpdateBudget();
    }

    RHIManagedMemoryCreateInfo RHIMemoryResidencyManagerVulkan::GetResourceResidencyInfo(RHIResource::Ptr res_ptr) const
    {

    }

    bool RHIMemoryResidencyManagerVulkan::IsUMA() const
    {
        return device_->IsUMA();
    }

    void RHIMemoryResidencyManagerVulkan::MakeResident(RHIResource::Ptr res_ptr, RHIManagedMemory& managed_mem, RHISizeType offset)
    {
        auto rhi_mem = reinterpret_cast<VkDeviceMemory>(managed_mem.parent_->rhi_mem_);
        const auto abs_offset = managed_mem.offset_ + offset;
        if (1) {
            vkBindBufferMemory(device_->Get(), ,rhi_mem, abs_offset);
        }
        else {
            vkBindImageMemory(device_->Get(), ,rhi_mem, abs_offset);
        }
    }

    void RHIMemoryResidencyManagerVulkan::MakeResident(RHIResource::Ptr res_ptr, RHIDedicatedMemoryBlock& dedicated_mem)
    {
        auto rhi_mem = reinterpret_cast<VkDeviceMemory>(dedicated_mem.rhi_mem_);
        if (1) {
            vkBindBufferMemory(device_->Get(), , rhi_mem, 0);
        }
        else
        {
            vkBindImageMemory(device_->Get(), , rhi_mem, 0);
        }
    }

    void RHIMemoryResidencyManagerVulkan::Evict(RHIResource::Ptr res_ptr)
    {
        
    }

    uint8_t RHIMemoryResidencyManagerVulkan::FindMemoryTypeIndex(RHIMemoryUsage usage, RHIMemoryFlags flags) const
    {

    }

    uint32_t RHIMemoryResidencyManagerVulkan::CalcPreferredBlockSizeHint(uint16_t heap_index)const
    {

    }

    void* RHIMemoryResidencyManagerVulkan::MallocRawMemory(uint32_t flags, uint64_t size, void* plat_data, void* user_data)
    {
        VkDeviceMemory memory;
        VkMemoryAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        alloc_info.allocationSize = size;
        alloc_info.memoryTypeIndex = 0u; //todo
        alloc_info.pNext = nullptr;
        vkAllocateMemory(device_->Get(), &alloc_info, g_host_alloc_vulkan, &memory);

        //todo buget and statistics 
        return memory; //todo
    }

    void RHIMemoryResidencyManagerVulkan::UpdateBudget() {
        mem_props_ = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
        mem_budget_ext_ = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
        VSChainPushFront(&mem_props_, &mem_budget_ext_);
        vkGetPhysicalDeviceMemoryProperties2(device_->GetPhyHandle(), &mem_props_);

        for (auto heap_index = 0u; heap_index < GetHeapCount(); ++heap_index) {
            
        }
    }

}
