#include "HALMemoryResidencyVulkan.h"
namespace Shard::HAL::Vulkan
{

    static inline bool IsTilingOptimal(HALMemoryFlags flags) {
        return (flags & EHALMemoryFlagBits::eTilingOptimal) == EHALMemoryFlagBits::eTilingOptimal;
    }

    void HALMemoryResidencyManagerVulkan::Init(HALGlobalEntity* rhi_entity)
    {
        auto* vulkan_rhi = static_cast<HALGlobalEntityVulkan*>(rhi_entity);
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
            const HALBlockVectorCreateFlags pool_flags{ EHALBlockVectorCreateFlagBits::eIgnoreBufferImageGanularity };
            managed_vec_.emplace_back(new HALManagedMemoryBlockVector((HALMemoryResidencyManager*)this, pool_flags, i, 0u, std::numeric_limits<uint32_t>::max(), preferred_size, 1.f, alloc_strategy));
            dedicated_vec_.emplace_back(new HALDedicatedMemoryBlockList((HALMemoryResidencyManager*)this, i));
        }
    }

    HALAllocationCreateInfo HALMemoryResidencyManagerVulkan::GetBufferResidencyInfo(HALBuffer* buffer_ptr) const
    {
        HALAllocationCreateInfo alloc_info{};
        VkMemoryRequirements* request = NewArray<VkMemoryRequirements>(1u);
        alloc_info.user_data_.reset(request, ArrayDeleter<VkMemoryRequirements>(1u));
        auto buffer_handle = (VkBuffer)(static_cast<HALBufferVulkan*>(buffer_ptr)->GetHandle());
        GetBufferMemoryRequirements(buffer_handle, *request, alloc_info.flags_);
        return alloc_info;
    }

    HALAllocationCreateInfo HALMemoryResidencyManagerVulkan::GetTextureResidencyInfo(HALTexture* texture_ptr) const
    {
        HALAllocationCreateInfo alloc_info{};
        VkMemoryRequirements* request = NewArray<VkMemoryRequirements>(1u);
        alloc_info.user_data_.reset(request, ArrayDeleter<VkMemoryRequirements>(1u));
        auto texture_handle = (VkImage)(static_cast<HALTextureVulkan*>(texture_ptr)->GetHandle());
        GetImageMemoryRequirements(texture_handle, *request, alloc_info.flags_);
        return alloc_info;
    }

    bool HALMemoryResidencyManagerVulkan::IsUMA() const
    {
        return device_->IsUMA();
    }

    void HALMemoryResidencyManagerVulkan::Evict(HALResource* res_ptr)
    {
        assert(res_ptr->IsBinded());
        auto allocation = res_ptr->GetBindMemory();
        //todo map / unmap ??
        DeAlloc(allocation);
        allocation = nullptr;
    }

    void* HALMemoryResidencyManagerVulkan::Map(HALResource* res_ptr,  HALAllocation* allocation)
    {
        assert(allocation != nullptr);
        if (!allocation->is_map_allowed_) {
            return nullptr;
        }
        if (IsMapped(allocation)) {
            return allocation->mapped_;
        }
        const auto pool_index = allocation->pool_index_;
        assert(pool_index < GetMemoryPoolCount());
        if (!IsDedicated(*allocation)) {
            auto* block = GetBlock(*allocation); 
            allocation->mapped_ = (void*)((std::uintptr_t)managed_vec_[pool_index]->MapBlock(*block) + allocation->managed_mem_.mem_.offset_);
        }
        else
        {
            auto* dedicated = GetDedicated(*allocation);
            MapRawMemory(dedicated->rhi_mem_, 0u, 0u, 0u, allocation->mapped_);
        }
    }

    void HALMemoryResidencyManagerVulkan::UnMap(HALResource* res_ptr, HALAllocation* allocation)
    {
        assert(allocation != nullptr);
        if (!allocation->is_map_allowed_|| !IsMapped(allocation)) {
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
            UnMapRawMemory(dedicated->rhi_mem_);
        }
    }

    void HALMemoryResidencyManagerVulkan::GetAllocationBackendSpecInfoApprox(HALAllocationCreateInfo& create_info)
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
        if (!(create_info.flags_ & EHALMemoryFlagBits::eTilingMask)) {
            create_info.flags_ &= EHALMemoryFlagBits::eForceDedicated;
        }
    }

    //todo the keypoint 
    uint8_t HALMemoryResidencyManagerVulkan::FindMemoryPoolIndex(HALMemoryUsage usage, HALMemoryFlags flags, void* user_data) const
    {
        const auto pool_count = GetMemoryPoolCount(); // mem_props_.memoryProperties.memoryHeapCount;
        uint32_t min_cost = std::numeric_limits<uint32_t>::max();
        uint8_t min_index{ std::numeric_limits<uint8_t>::max() };
        const auto* memory_request = user_data ? reinterpret_cast<VkMemoryRequirements*>(user_data) : nullptr;

        VkMemoryPropertyFlags required_flags{ 0u }, preferred_flags{ 0u }, not_preferred_flags{ 0u };

        const auto convert_flags = [&]()->bool {
            if (IsUMA() && (flags & EHALMemoryFlagBits::eDeviceLocal)) {
                return false;
            }
            if (!IsUMA() && (usage & EHALMemoryUsageBits::eShader)) {
                required_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                not_preferred_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            }
            if (!IsUMA() && ((usage & EHALMemoryUsageBits::eCopyDst) && (flags & EHALMemoryFlagBits::eDeviceLocal))) {
                required_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                not_preferred_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            }

            if ((usage & EHALMemoryUsageBits::eCopySrc)) {
                if (flags & EHALMemoryFlagBits::eHostVisible) {
                    required_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                    not_preferred_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                }
            }

            if (flags & EHALMemoryFlagBits::eAllowMapped) {
                required_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT; //todo
            }
            //for usage:unkown 
            if (flags & EHALMemoryFlagBits::eHostVisible){
                required_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                preferred_flags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            }
            if (flags & EHALMemoryFlagBits::eHostCached) {
                required_flags != VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            }
            if (IsUMA() && (flags & EHALMemoryFlagBits::eLazily)) {
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

    /*how to deal with heap smaller than preferred size: Memory Management in Vulkan and DX12*/
    HALSizeType HALMemoryResidencyManagerVulkan::CalcPreferredBlockSizeHint(uint16_t pool_index)const
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

    uint32_t HALMemoryResidencyManagerVulkan::GetMaxAllocationCount() const
    {
        VkPhysicalDeviceProperties props;
        device_->GetPhysicalDeviceProperties(props);
        return props.limits.maxMemoryAllocationCount;
    }

    void HALMemoryResidencyManagerVulkan::UpdateMemoryBudget()
    {
        HALMemoryResidencyManager::UpdateMemoryBudget();

        //vulkan budget update 
        VkPhysicalDeviceMemoryProperties2 mem_props2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
        VkPhysicalDeviceMemoryBudgetPropertiesEXT mem_budget_ext{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
        const auto is_budget_ext_enabled = GET_PARAM_TYPE_VAL(BOOL, DEVICE_MEMORY_BUDGET);

        if (is_budget_ext_enabled) {
            VSChainPushFront(&mem_props_, &mem_budget_ext);
        }
        device_->GetPhysicalDeviceMemoryProperties2(mem_props2);
        mem_props_ = mem_props2.memoryProperties;

        for (auto pool_index = 0u; pool_index < GetMemoryPoolCount(); ++pool_index) {
            const auto type_index = GetTypeIndexFromPoolIndex(pool_index);
            const auto heap_index = mem_props_.memoryTypes[type_index].heapIndex;
            if (is_budget_ext_enabled) {
                mem_budget_.pool_budget_[pool_index].usage_ = mem_budget_ext.heapUsage[heap_index];
                mem_budget_.pool_budget_[pool_index].budget_ = mem_budget_ext.heapBudget[heap_index];
            }
            else
            {
                //if ext not enable, use heap max size as budget
                mem_budget_.pool_budget_[pool_index].usage_ =  mem_budget_.pool_budget_[pool_index].allocation_bytes_.load();
                mem_budget_.pool_budget_[pool_index].budget_ = mem_props_.memoryHeaps[heap_index].size;
            }
        }
    }

    void HALMemoryResidencyManagerVulkan::MakeBufferResidentImpl(HALBuffer* res_ptr, void* rhi_mem, HALSizeType offset)  
    {

        auto buffer_handle = (VkBuffer)(static_cast<HALBufferVulkan*>(res_ptr)->GetHandle());
        device_->BindBufferMemory(buffer_handle, (VkDeviceMemory)rhi_mem, offset);
    }

    void HALMemoryResidencyManagerVulkan::MakeTextureResidentImpl(HALTexture* res_ptr, void* rhi_mem, HALSizeType offset) 
    {
        auto texture_handle = (VkImage)(static_cast<HALTextureVulkan*>(res_ptr)->GetHandle());
        device_->BindImageMemory(texture_handle, (VkDeviceMemory)rhi_mem, offset);
    }

    void* HALMemoryResidencyManagerVulkan::MallocRawMemory(uint32_t pool_index, uint64_t size, float priority, void* plat_data, void* user_data)
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
   
        if (AccquireMemoryBudgetBlock(pool_index, size)) {

            device_->AllocMemory(memory, alloc_info);
            return memory;
        }
        LOG(ERROR) << fmt::format("out of memory error while allocating vulkan memory pool{} with size {}", pool_index, size);
        return nullptr;
    }

    void HALMemoryResidencyManagerVulkan::FreeRawMemory(void* memory, uint32_t pool_index, HALSizeType offset, HALSizeType size, void*& mapped)
    {
        device_->DeAllocMemory((VkDeviceMemory)memory);
        ReleaseMemoryBudgetAllocation(pool_index, size);
    }

    void HALMemoryResidencyManagerVulkan::MapRawMemory(void* memory, HALSizeType offset, HALSizeType size, uint32_t flags, void*& mapped)
    {
        //don't use placed map flag, set flags to zero as default
        mapped = device_->MapMemory((VkDeviceMemory)memory, offset, size, 0u);
    }

    void HALMemoryResidencyManagerVulkan::UnMapRawMemory(void* memory)
    {
        device_->UnMapMemory((VkDeviceMemory)memory);
    }

    void HALMemoryResidencyManagerVulkan::GetBufferMemoryRequirements(VkBuffer buffer, VkMemoryRequirements& requirements, HALMemoryFlags& flags)const
    {
        VkBufferMemoryRequirementsInfo2KHR buffer_req_info{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR, buffer};
        VkMemoryDedicatedRequirementsKHR dedicated_req_info{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR };
        VkMemoryRequirements2KHR mem_req2_info{ VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR };
        VSChainPushFront(&mem_req2_info, &dedicated_req_info);
        device_->GetBufferMemoryRequirements2(buffer_req_info, mem_req2_info);

        //broadcast requirement
        requirements = mem_req2_info.memoryRequirements;
        flags &= (dedicated_req_info.requiresDedicatedAllocation ? EHALMemoryFlagBits::eForceDedicated : 0u);
    }

    void HALMemoryResidencyManagerVulkan::GetImageMemoryRequirements(VkImage image, VkMemoryRequirements& requirements, HALMemoryFlags& flags)const
    {
        VkImageMemoryRequirementsInfo2KHR image_req_info{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR, image };
        VkMemoryDedicatedRequirementsKHR dedicated_req_info{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR };
        VkMemoryRequirements2KHR mem_req2_info{ VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR };
        VSChainPushFront(&mem_req2_info, &dedicated_req_info);
        device_->GetImageMemoryRequirements2(image_req_info, mem_req2_info);

        requirements = mem_req2_info.memoryRequirements;
        flags &= (dedicated_req_info.requiresDedicatedAllocation ? EHALMemoryFlagBits::eForceDedicated : 0u);
    }

    uint32_t HALMemoryResidencyManagerVulkan::GetPoolIndexFromTypeIndex(uint32_t type_index, bool is_optimal) const
    {
        return type_index * VULKAN_TILING_CATEGORY + !!is_optimal; 
    }

    uint32_t HALMemoryResidencyManagerVulkan::GetTypeIndexFromPoolIndex(uint32_t pool_index) const
    {
        return pool_index / VULKAN_TILING_CATEGORY;
    }

}
