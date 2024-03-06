#include "RHIMemoryResidencyVulkan.h"

namespace Shard::RHI::Vulkan
{

    static inline bool IsTilingOptimal(RHIMemoryFlags flags) {
        return (flags & ERHIMemoryFlagBits::eTilingOptimal) == ERHIMemoryFlagBits::eTilingOptimal;
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
        const auto pool_count = mem_props_.memoryTypeCount;
        const auto alloc_strategy = GET_PARAM_TYPE_VAL(UINT, RESIDENCY_ALLOCATE_STRATEGY);
        for (auto i = 0; i < pool_count; ++i) {
            const auto& heap_props = mem_props_.memoryTypes[i];
            const auto preferred_size = CalcPreferredBlockSizeHint(i);
            const RHIBlockVectorCreateFlags pool_flags{ ERHIBlockVectorCreateFlagBits::eIgnoreBufferImageGanularity };
            managed_vec_.emplace_back(new RHIManagedMemoryBlockVector((RHIMemoryResidencyManager*)this, pool_flags, i, 0u, std::numeric_limits<uint32_t>::max(), preferred_size, 1.f, alloc_strategy));
            dedicated_vec_.emplace_back(new RHIDedicatedMemoryBlockList((RHIMemoryResidencyManager*)this, i));
        }
    }

    RHIAllocationCreateInfo RHIMemoryResidencyManagerVulkan::GetResourceResidencyInfo(RHIResource::Ptr res_ptr) const
    {
        RHIAllocationCreateInfo alloc_info{};
        VkMemoryRequirements* request = NewArray<VkMemoryRequirements>(1u);
        alloc_info.user_data_.reset(request, ArrayDeleter<VkMemoryRequirements>(1u));

        if (1) {
            GetBufferMemoryRequirements(res_ptr, *request, alloc_info.flags_);
        }
        else {
            GetImageMemoryRequirements(res_ptr, *request, alloc_info.flags_);
        }

        return alloc_info;
    }

    bool RHIMemoryResidencyManagerVulkan::IsUMA() const
    {
        return device_->IsUMA();
    }

    void RHIMemoryResidencyManagerVulkan::MakeResident(RHIResource::Ptr res_ptr, RHIAllocation& allocation)
    {
        if (res_ptr->IsDedicated()) {
            assert(IsDedicated(allocation));
            MakeResident(res_ptr, allocation.dedicated_mem_);
        }
        else
        {
            MakeResident(res_ptr, allocation.managed_mem_.mem_, allocation.managed_mem_.offset_);
        }
    }

    void RHIMemoryResidencyManagerVulkan::MakeResident(RHIResource::Ptr res_ptr, RHIManagedMemory& managed_mem, RHISizeType offset)
    {
        auto rhi_mem = reinterpret_cast<VkDeviceMemory>(managed_mem.parent_->rhi_mem_);
        const auto abs_offset = managed_mem.offset_ + offset;
        if (1) {
            vkBindBufferMemory(device_->Get(), , rhi_mem, abs_offset);
        }
        else {
            vkBindImageMemory(device_->Get(), , rhi_mem, abs_offset);
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
        assert(res_ptr->IsBinded());
        auto allocation = res_ptr->GetBindMemory();
        //todo map / unmap ??
        DeAlloc(allocation);
        allocation = nullptr;
    }

    void* RHIMemoryResidencyManagerVulkan::Map(RHIResource::Ptr res_ptr)
    {
        auto* allocation = res_ptr->GetBindMemory();
        if (!allocation->is_map_allowed_) {
            return nullptr;
        }
        const auto pool_index = allocation->pool_index_;
        assert(pool_index < GetMemoryPoolCount());
        if (!IsDedicated(*allocation)) {
            auto* block = GetBlock(*allocation); 
            managed_vec_[pool_index]->MapBlock(*block);
        }
        else
        {
            auto* dedicated = GetDedicated(*allocation);
            dedicated_vec_[pool_index]->MapBlock(*dedicated);
        }
    }

    void RHIMemoryResidencyManagerVulkan::UnMap(RHIResource::Ptr res_ptr)
    {
        auto* allocation = res_ptr->GetBindMemory();
        if (!allocation->is_map_allowed_) {
            return;
        }
        const auto pool_index = allocation->pool_index_;
        assert(pool_index < GetMemoryPoolCount());
        if (!IsDedicated(*allocation)) {
            auto* block = GetBlock(*allocation);
            managed_vec_[pool_index]->UnMapBlock(*block);
        }
        else
        {
            auto* dedicated = GetDedicated(*allocation);
            dedicated_vec_[pool_index]->UnMapBlock(*dedicated);
        }
    }

    void RHIMemoryResidencyManagerVulkan::GetAllocationBackendSpecInfoApprox(RHIAllocationCreateInfo& create_info)
    {
        VkMemoryRequirements* request = NewArray<VkMemoryRequirements>(1u);
        request->alignment = create_info.alignment_;
        request->size = create_info.size_;
        //transform create_info usage and flags to request flags
        auto pool_index = FindMemoryPoolIndex(create_info.type_, create_info.flags_, nullptr);
        if (pool_index == std::numeric_limits<uint8_t>::max()) {
            LOG(ERROR) << "find invalid heap index while getting allocation info approx";
        }
        /*
        * memoryTypeBits is a bitmask and contains one bit set for every supported memory type for the resource.
        * Bit i is set if and only if the memory type i in the VkPhysicalDeviceMemoryProperties structure for the
        * physical device is supported for the resource.
        */
        request->memoryTypeBits = 1U << pool_index; //?
        create_info.user_data_.reset(request, ArrayDeleter<VkMemoryRequirements>(1u));
        /*if do not know whether allcating a optimal resource, allocate it as dedicated*/
        if (!(create_info.flags_ & ERHIMemoryFlagBits::eTilingMask)) {
            create_info.flags_ &= ERHIMemoryFlagBits::eForceDedicated;
        }
    }

    //todo the keypoint 
    uint8_t RHIMemoryResidencyManagerVulkan::FindMemoryPoolIndex(RHIMemoryUsage usage, RHIMemoryFlags flags, void* user_data) const
    {
        const auto pool_count = GetMemoryPoolCount(); // mem_props_.memoryProperties.memoryHeapCount;
        uint32_t min_cost = std::numeric_limits<uint32_t>::max();
        uint8_t min_index{ std::numeric_limits<uint8_t>::max() };
        const auto* memory_request = user_data ? reinterpret_cast<VkMemoryRequirements*>(user_data) : nullptr;

        VkMemoryPropertyFlags required_flags{ 0u }, preferred_flags{ 0u }, not_preferred_flags{ 0u };

        const auto convert_flags = [&]()->bool {
            if (IsUMA() && (flags & ERHIMemoryFlagBits::eDeviceLocal)) {
                return false;
            }
            if (!IsUMA() && (usage & (ERHIMemoryUsageBits::eUAV | ERHIMemoryUsageBits::eRTV | ERHIMemoryUsageBits::eSRV))) {
                required_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                not_preferred_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            }
            if (!IsUMA() && ((usage & ERHIMemoryUsageBits::eCopyDst) && (flags & ERHIMemoryFlagBits::eDeviceLocal))) {
                required_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            }

            if ((usage & ERHIMemoryUsageBits::eCopySrc)) {
                not_preferred_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            }

            if (flags & ERHIMemoryFlagBits::eAllowMapped) {
                required_flags |= 0u; //todo
            }
            //for usage:unkown 
            if (flags & ERHIMemoryFlagBits::eHostVisible){
                required_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                preferred_flags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            }
            if (flags & ERHIMemoryFlagBits::eHostCached) {
                required_flags != VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            }
            if (IsUMA() && (flags & ERHIMemoryFlagBits::eLazily)) {
                required_flags != VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
            }
            else
            {
                //not_preferred_flags != VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
            }
            
            /*
            if (memory_request) {
                //todo
            }
            else
            {
                //todo memory requirement generate by guess
            }
            */
            return true;
        }; 
        if (!convert_flags()) {
            return std::numeric_limits<uint8_t>::max();
        }
        for (auto pool_index = 0u; pool_index < pool_count; ++pool_index)
        {
            const auto type_index = GetTypeIndexFromPoolIndex(pool_index);
            const auto pool_flags = mem_props_.memoryTypes[type_index].propertyFlags;
            if (~pool_flags & required_flags)
                continue;
            if (memory_request && !(memory_request->memoryTypeBits & (1U << pool_index))) {
                LOG(ERROR) << "should already reject this heap in last step";
                continue;
            }
            const auto heap_cost = Utils::CountBits(~pool_flags&preferred_flags) + Utils::CountBits(pool_flags&not_preferred_flags); //todo do we need budget cost ?
            if (heap_cost < min_cost) {
                min_cost = heap_cost;
                min_index = pool_index;
                if (!min_cost) {
                    break;
                }
            }
        }
        
        if (min_index != std::numeric_limits<uint8_t>::max()) {
            //for linear and optimal resource allocate in seperate pool
            min_index = GetPoolIndexFromTypeIndex(min_index, IsTilingOptimal(flags));
        }
        return min_index;
    }

    /*how to deal with heap smaller than preferred size*/
    RHISizeType RHIMemoryResidencyManagerVulkan::CalcPreferredBlockSizeHint(uint16_t pool_index)const
    {
        static constexpr auto SMALL_HEAP_MAX_SIZE = 1024ULL * 1024 * 1024;
        const auto heap_index = mem_props_.memoryTypes[pool_index].heapIndex;
        const auto heap_size = mem_props_.memoryHeaps[heap_index].size;
        const auto preferred_size = GET_PARAM_TYPE_VAL(UINT, RESIDENCY_PREFER_BLOCK_SIZE);
        if (heap_size > SMALL_HEAP_MAX_SIZE) {
            return preferred_size;
        }
        else {
            return  Utils::AlignUp(heap_size / 8, 32); //vma use 1/8 of heap size
        }
    }

    uint32_t RHIMemoryResidencyManagerVulkan::GetMaxAllocationCount() const
    {
        return device_->GetPhysicalDeviceProperties().limits.maxMemoryAllocationCount;
    }

    void RHIMemoryResidencyManagerVulkan::UpdateMemoryBudget()
    {
        RHIMemoryResidencyManager::UpdateMemoryBudget();

        //vulkan budget update 
        mem_props_ = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
        mem_budget_ext_ = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
        VSChainPushFront(&mem_props_, &mem_budget_ext_);
        vkGetPhysicalDeviceMemoryProperties2(device_->GetPhyHandle(), &mem_props_);

        for (auto pool_index = 0u; pool_index < GetMemoryPoolCount(); ++pool_index) {
            const auto type_index = GetTypeIndexFromPoolIndex(pool_index);
            const auto heap_index = mem_props_.memoryTypes[type_index].heapIndex;
            mem_budget_.pool_budget_[pool_index].usage_ = mem_budget_ext_.heapUsage[heap_index];
            mem_budget_.pool_budget_[pool_index].budget_ = mem_budget_ext_.heapBudget[heap_index];
        }
    }
    void* RHIMemoryResidencyManagerVulkan::MallocRawMemory(uint32_t pool_index, uint64_t size, float priority, void* plat_data, void* user_data)
    {
        VkDeviceMemory memory;
        VkMemoryAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        alloc_info.allocationSize = size;
        alloc_info.memoryTypeIndex = GetTypeIndexFromPoolIndex(pool_index);
        alloc_info.pNext = nullptr;
#if defined(VK_EXT_memory_priority)
        VkMemoryPriorityAllocateInfoEXT priority_info{ VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT };
        priority_info.priority = std::clamp(priority, 0.f, 1.f);
        priority_info.pNext = nullptr;
        VSChainPushFront(&alloc_info, &priority_info);
#endif
        vkAllocateMemory(device_->Get(), &alloc_info, g_host_alloc_vulkan, &memory);

        //todo buget and statistics 
        return memory; //todo
    }

    void RHIMemoryResidencyManagerVulkan::FreeRawMemory(void* memory, RHISizeType offset, RHISizeType size, void*& mapped)
    {
        vkFreeMemory(device_->Get(), (VkDeviceMemory)memory, g_host_alloc_vulkan);
    }

    void RHIMemoryResidencyManagerVulkan::MapRawMemory(void* memory, RHISizeType offset, RHISizeType size, uint32_t flags, void*& mapped)
    {
        //do not use placed map flag
        vkMapMemory(device_->Get(), (VkDeviceMemory)memory, offset, (size ? size : VK_WHOLE_SIZE), 0u, &mapped);
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

    uint32_t RHIMemoryResidencyManagerVulkan::GetPoolIndexFromTypeIndex(uint32_t type_index, bool is_optimal) const
    {
        return type_index * VULKAN_TILING_CATEGORY + !!is_optimal; 
    }

    uint32_t RHIMemoryResidencyManagerVulkan::GetTypeIndexFromPoolIndex(uint32_t pool_index) const
    {
        return pool_index / VULKAN_TILING_CATEGORY;
    }

}
