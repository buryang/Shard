#include "RHIMemoryResidencyVulkan.h"

namespace Shard::RHI::Vulkan
{
    static bool FindMemoryPreferences(bool is_uma, const RHIAllocationCreateInfo& alloc_info, VkMemoryPropertyFlags& preferred_flags, VkMemoryPropertyFlags& required_flags, VkMemoryPropertyFlags& not_preferred_flags)
    {

    }
    void RHIMemoryResidencyManagerVulkan::Init(RHIGlobalEntity::Ptr rhi_entity)
    {
        auto* vulkan_rhi = static_cast<RHIGlobalEntityVulkan::Ptr>(rhi_entity);
        assert(vulkan_rhi != nullptr);
        instance_ = vulkan_rhi->GetVulkanInstance();
        device_ = vulkan_rhi->GetVulkanDevice();
        //refresh memory props
        UpdateMemoryBudget();
         
        //create blockvector and dedicatedlist
        const auto heap_count = mem_props_.memoryProperties.memoryHeapCount;
        for (auto i = 0; i < heap_count; ++i) {
            const auto& heap_props = mem_props_.memoryProperties.memoryHeaps[i];
            managed_vec_.emplace_back(new RHIManagedMemoryBlockVector((RHIMemoryResidencyManager*)this, i, ));
            dedicated_vec_.emplace_back(new RHIDedicatedMemoryBlockList((RHIMemoryResidencyManager*)this, i));
        }
    }

    RHIAllocationCreateInfo RHIMemoryResidencyManagerVulkan::GetResourceResidencyInfo(RHIResource::Ptr res_ptr) const
    {
        RHIAllocationCreateInfo alloc_info{};

    }

    bool RHIMemoryResidencyManagerVulkan::IsUMA() const
    {
        return device_->IsUMA();
    }

    void RHIMemoryResidencyManagerVulkan::MakeResident(RHIResource::Ptr res_ptr, RHIAllocation& allocation, RHISizeType offset)
    {
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
            vkBindBufferMemory(device_->Get(),  , rhi_mem, 0);
        }
        else
        {
            vkBindImageMemory(device_->Get(), , rhi_mem, 0);
        }
    }

    void RHIMemoryResidencyManagerVulkan::Evict(RHIResource::Ptr res_ptr)
    {
        
    }

    void* RHIMemoryResidencyManagerVulkan::Map(RHIResource::Ptr res_ptr)
    {
    }

    void RHIMemoryResidencyManagerVulkan::UnMap(RHIResource::Ptr res_ptr)
    {

    }

    void RHIMemoryResidencyManagerVulkan::GetAllocationBackendSpecInfoApprox(RHIAllocationCreateInfo& create_info)
    {

    }

    uint8_t RHIMemoryResidencyManagerVulkan::FindMemoryTypeIndex(RHIMemoryUsage usage, RHIMemoryFlags flags) const
    {
        const auto heap_count = mem_props_.memoryProperties.memoryHeapCount;
        for (auto heap_index = 0u; heap_index < heap_count; ++heap_index)
        {
            //to do
        }
        return -1;
    }

    /*how to deal with heap smaller than preferred size*/
    RHISizeType RHIMemoryResidencyManagerVulkan::CalcPreferredBlockSizeHint(uint16_t heap_index)const
    {
        static constexpr auto SMALL_HEAP_MAX_SIZE = 1024ULL * 1024 * 1024;
        const auto heap_size = mem_props_.memoryProperties.memoryHeaps->size;
        const auto preferred_size = GET_PARAM_TYPE_VAL(UINT, RESIDENCY_PREFER_BLOCK_SIZE);
        if (heap_size > SMALL_HEAP_MAX_SIZE) {
            return preferred_size;
        }
        else {
            return  Utils::AlignUp(heap_size / 8, 32); //vma use 1/8 of heap size
        }
    }

    void RHIMemoryResidencyManagerVulkan::UpdateMemoryBudget()
    {
        RHIMemoryResidencyManager::UpdateMemoryBudget();

        //vulkan budget update 
        mem_props_ = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
        mem_budget_ext_ = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
        VSChainPushFront(&mem_props_, &mem_budget_ext_);
        vkGetPhysicalDeviceMemoryProperties2(device_->GetPhyHandle(), &mem_props_);

        for (auto heap_index = 0u; heap_index < GetHeapCount(); ++heap_index) {
            mem_budget_.heap_budget_[heap_index].usage_ = mem_budget_ext_.heapUsage[heap_index];
            mem_budget_.heap_budget_[heap_index].budget_ = mem_budget_ext_.heapBudget[heap_index];
        }
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

    void RHIMemoryResidencyManagerVulkan::FreeRawMemory(void* memory)
    {
        vkFreeMemory(device_->Get(), (VkDeviceMemory)memory, g_host_alloc_vulkan);
    }

    void RHIMemoryResidencyManagerVulkan::MapRawMemory(void* memory, RHISizeType offset, RHISizeType size, uint32_t flags, void*& mapped)
    {
        //do not use placed map flag
        vkMapMemory(device_->Get(), (VkDeviceMemory)memory, offset, size, 0u, &mapped);
    }

    void RHIMemoryResidencyManagerVulkan::UnMapRawMemory(void* memory)
    {
        vkUnmapMemory(device_->Get(), (VkDeviceMemory)memory);
    }

    void RHIMemoryResidencyManagerVulkan::GetBufferMemoryRequirements(VkBuffer buffer, VkMemoryRequirements& requirements, RHIMemoryFlags& flags)
    {
        VkBufferMemoryRequirementsInfo2KHR buffer_req_info{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR, buffer};
        VkMemoryDedicatedRequirementsKHR dedicated_req_info{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR };
        VkMemoryRequirements2KHR mem_req2_info{ VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR };
        VSChainPushFront(&mem_req2_info, &dedicated_req_info);
        vkGetBufferMemoryRequirements2KHR(device_->Get(), &buffer_req_info, &mem_req2_info);

        //broadcasst requirement
        requirements = mem_req2_info.memoryRequirements;
        flags &= (dedicated_req_info.requiresDedicatedAllocation ? ERHIMemoryFlagBits::eForceDedicated : 0u);
    }

    void RHIMemoryResidencyManagerVulkan::GetImageMemoryRequirements(VkImage image, VkMemoryRequirements& requirements, RHIMemoryFlags& flags)
    {
        VkImageMemoryRequirementsInfo2KHR image_req_info{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR, image };
        VkMemoryDedicatedRequirementsKHR dedicated_req_info{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR };
        VkMemoryRequirements2KHR mem_req2_info{ VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR };
        VSChainPushFront(&mem_req2_info, &dedicated_req_info);
        vkGetImageMemoryRequirements2KHR(device_->Get(), &image_req_info, &mem_req2_info);

        requirements = mem_req2_info.memoryRequirements;
        flags &= (dedicated_req_info.requiresDedicatedAllocation ? ERHIMemoryFlagBits::eForceDedicated : 0u);
    }

}
