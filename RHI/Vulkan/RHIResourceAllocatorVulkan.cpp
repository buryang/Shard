#include "RHI/Vulkan/RHIResourcesVulkan.h"
#include "RHI/Vulkan/RHIResourceAllocatorVulkan.h"
#include <iomanip>

namespace MetaInit::RHI::Vulkan {
	//whether use dedicate memory allocation
	REGIST_PARAM_TYPE(BOOL, VULKAN_DEDICATED_ALLOC, true);

	/*linear and non-linear resources must be placed in adjacent memory locations to avoid aliasing.*/
	static inline VkDeviceSize GetBufferImageGranularity() {
		const auto& phy_props = RHI::Vulkan::RHIGlobalEntityVulkan::Instance()->GetVulkanDevice()->GetPhysicalDeviceProperties();
		return phy_props.limits.bufferImageGranularity;
	}

	/*That is, the end of the first resource (A) and the beginning of the second resource (B) must be on
		separate ¡°pages¡± of size bufferImageGranularity. bufferImageGranularity may be different than the
		physical page size of the memory heap. PS:and will be used simultaneously*/
	static inline bool IsResourceAlias(VkDeviceSize lhs_offset, VkDeviceSize lhs_size, VkDeviceSize rhs_offset) {
		const auto granularity_mask = ~(GetBufferImageGranularity()-1);
		const auto lhs_end_page = (lhs_offset + lhs_size - 1) & granularity_mask;
		const auto rhs_start_page = rhs_offset & granularity_mask;
		return lhs_end_page == rhs_start_page;
	}

	/*FIXME to simplify code logic, do ganularity for every resource here MUST BE FIXMED*/
	static inline MemoryAllocRequirements PreProcessAllocRequirements(const MemoryAllocRequirements& input) {
		MemoryAllocRequirements output{ input };
		const auto ganularity = GetBufferImageGranularity();
		output.alignment_ = std::max(input.alignment_, ganularity);
		output.size_ = Utils::AlignUp(input.size_, ganularity);
		return output;
	}

	static inline int32_t FindMemoryTypeIndexRequired(uint32_t type_bits_req, VkMemoryPropertyFlags prop_req) {
		VkPhysicalDeviceMemoryProperties mem_props;
		vkGetPhysicalDeviceMemoryProperties(GetGlobalPhyDevice(), &mem_props);
		for (auto n = 0; n < mem_props.memoryTypeCount; ++n) {
			const auto type_bits = 1 << n;
			const bool is_type_req = type_bits & type_bits_req;
			const bool has_prop_req = (mem_props.memoryTypes[n].propertyFlags & prop_req) == prop_req;
			if (is_type_req && has_prop_req) {
				return n;
			}
		}
		return -1;
	}

	static inline MemoryAllocRequirements GetTextureMemoryRequirements(VkImage texture, VkMemoryPropertyFlags optimal_flags, VkMemoryPropertyFlags reuire_flags) {
		VkMemoryRequirements mem_req;
		vkGetImageMemoryRequirements(GetGlobalDevice(), texture, &mem_req);

		MemoryAllocRequirements alloc_req{ .size_ = mem_req.size };
		//FIXME
		auto mem_type = FindMemoryTypeIndexRequired(mem_req.memoryTypeBits, optimal_flags); //optimal
		if (-1 == mem_type) {
			mem_type = FindMemoryTypeIndexRequired(mem_req.memoryTypeBits, reuire_flags); //require
		}
		assert(mem_type != -1 && "texture reuired flags bits must found");
		alloc_req.mem_type_ = mem_type;
		return PreProcessAllocRequirements(alloc_req);
	}

	static inline MemoryAllocRequirements GetBufferMemoryRequirements(VkBuffer buffer, VkMemoryPropertyFlags optimal_flags, VkMemoryPropertyFlags reuire_flags) {
		VkMemoryRequirements mem_req;
		vkGetBufferMemoryRequirements(GetGlobalDevice(), buffer, &mem_req);
		MemoryAllocRequirements alloc_req{ .size_ = mem_req.size };
		//FIXME
		auto mem_type = FindMemoryTypeIndexRequired(mem_req.memoryTypeBits, optimal_flags); //optimal
		if (-1 == mem_type) {
			mem_type = FindMemoryTypeIndexRequired(mem_req.memoryTypeBits, reuire_flags);
		}
		assert(mem_type != -1 && "texture reuired flags bits must found");
		alloc_req.mem_type_ = mem_type;
		return PreProcessAllocRequirements(alloc_req);
	}

	bool RHIPooledTextureAllocatorVulkan::AllocTexture(const RHITextureInitializer& desc, RHITextureVulkan::Ptr texture)
	{
		RHIPooledTextureAllocatorVulkan::PooledResource pooled_resource;
		if (auto ret = FindSuitAbleResource(true, &desc, pooled_resource)) {
			*texture = std::move(*static_cast<RHITextureVulkan::Ptr>(pooled_resource.handle_));
			return true;
		}

		texture->SetUpHandleAlone();
		auto alloc_req = GetTextureMemoryRequirements(texture->GetImpl()->Get(), 0x0, 0x0);
		if (desc.is_dedicated_ && GET_PARAM_TYPE_VAL(BOOL, DEVICE_DEDICATED_ALLOC)) {
			auto& mem_stat = RHIDeviceMemoryStateCounter::Instance();
			mem_stat.InCreAllocCount();
			PCHECK(alloc_req.size_ < mem_stat.GetHeapBudget(mem_stat.GetHeapIndexForMemType(alloc_req.mem_tpye_)));
			MemoryAllocation allocation;
			VkMemoryDedicatedAllocateInfoKHR dedicate_info{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
			dedicate_info.image = 0;
			VkMemoryAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			alloc_info.allocationSize = alloc_req.size_;
			alloc_info.memoryTypeIndex = alloc_req.mem_tpye_;
			alloc_info.pNext = &dedicate_info;
			vkAllocateMemory(GetGlobalDevice(), &alloc_info, g_host_alloc, &allocation.mem_);
			texture->SetUpHandleMemory(allocation);
		}
		else
		{
			auto allocation = alloc_->Alloc(alloc_req);
			texture->SetUpHandleMemory(allocation);
		}
		return true;
	}

	bool RHIPooledTextureAllocatorVulkan::AllocBuffer(const RHIBufferInitializer& desc, RHIBufferVulkan::Ptr buffer)
	{
		RHIPooledTextureAllocatorVulkan::PooledResource resource;
		if(auto ret = FindSuitAbleResource(false, &desc, resource))
		{
			*buffer = std::move(*static_cast<RHIBufferVulkan::Ptr>(buffer));
			return true;
		}
		
		buffer->SetUpHandleAlone();
		auto alloc_req = GetBufferMemoryRequirements(buffer->GetImpl()->Get(), 0x0, 0x0);
		if (desc.is_dedicated_ && GET_PARAM_TYPE_VAL(BOOL, DEVICE_DEDICATED_ALLOC)) {
			auto& mem_stat = RHIDeviceMemoryStateCounter::Instance();
			mem_stat.InCreAllocCount();
			PCHECK(alloc_req.size_ < mem_stat.GetHeapBudget(mem_stat.GetHeapIndexForMemType(alloc_req.mem_type_));
			MemoryAllocation allocation;
			VkMemoryDedicatedAllocateInfoKHR dedicate_info{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO };
			dedicate_info.buffer = 0;
			VkMemoryAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			alloc_info.allocationSize = alloc_req.size_;
			alloc_info.memoryTypeIndex = alloc_req.mem_tpye_;
			alloc_info.pNext = &dedicate_info;
			vkAllocateMemory(GetGlobalDevice(), &alloc_info, g_host_alloc, &allocation.mem_);
			buffer->SetUpHandleMemory(allocation);
		}
		else
		{
			auto allocation = alloc_->Alloc(alloc_req);
			buffer->SetUpHandleMemory(allocation);
		}
		return true;
	}

	void RHIPooledTextureAllocatorVulkan::Free(RHIResource::Ptr resource)
	{
		PooledResource pooled_resource{ .handle_ = resource, .last_stamp_ = curr_time_stamp_ };
		//FIXME restore desc
		pooled_resource.buffer_desc_ = {};
		pooled_resource.texture_desc_ = {};
		pooled_repo_.emplace_back(pooled_resource);
	}

	bool RHIPooledTextureAllocatorVulkan::Tick(float time)
	{
		bool force_release = false;
		auto max_time_gap = GET_PARAM_TYPE_VAL(FLOAT, RHI_POOLED_TIME_GAP);
		do
		{
			for (auto iter = pooled_repo_.begin(); iter != pooled_repo_.end(); ++iter) {
				if (time - iter->LastStamp() > max_time_gap) {
					pooled_repo_.erase(iter);
				}
			}
			//list size() is slow!!
			if (pooled_repo_.size() < GET_PARAM_TYPE_VAL(UINT, RHI_POOLED_MAX_RES) || !force_release){
				break;
			}
			force_release = true;
			max_time_gap *= 0.8;
		} while (1);

		curr_time_stamp_ = time;
		return true;
	}

	bool RHIPooledTextureAllocatorVulkan::FindSuitAbleResource(bool is_texture, const void* desc, RHIPooledTextureAllocatorVulkan::PooledResource& ret)
	{
		bool found = false;
		if (is_texture) {
			auto try_time = 2;
			const auto texture_desc = *reinterpret_cast<const RHITextureInitializer*>(desc);
			const auto is_texture_convertable = [&](const RHITextureInitializer& rhs) {
				return rhs.layout_ == texture_desc.layout_ && rhs.format_ == texture_desc.format_;
			};
			while (try_time-- > 0 && !found) {
				for (auto iter = pooled_repo_.begin(); iter != pooled_repo_.end(); ++iter) {
					if (try_time > 0 && iter->texture_desc_ == texture_desc) {
						found = true;
						ret = *iter;
						pooled_repo_.erase(iter);
					}
					else if (is_texture_convertable(iter->texture_desc_)) {
						found = true;
						ret = *iter;
						pooled_repo_.erase(iter);
						//force transimit
						ret.texture_desc_.access_ = Renderer::EAccessFlags::eNone;
					}
				}
			}
		}
		else
		{
			const auto buffer_desc = *reinterpret_cast<const RHIBufferInitializer*>(desc);
			//find best fit buffer block
			VkDeviceSize best_fit_size = VK_WHOLE_SIZE;
			auto best_fit_iter = pooled_repo_.begin();
			const auto is_buffer_fit = [&](auto rhs_desc) {
				return rhs_desc.access_ == buffer_desc.access_ &&
					rhs_desc.size_ >= buffer_desc.size_ &&
					rhs_desc.type_ == buffer_desc.type_ &&
					rhs_desc.is_dedicated_ == buffer_desc.is_dedicated_ &&
					rhs_desc.is_transiant_ == buffer_desc.is_transiant_; //??
			};
			for (auto iter = pooled_repo_.begin(); iter != pooled_repo_.end(); ++iter) {
				if (is_buffer_fit(iter->buffer_desc_)&& iter->buffer_desc_.size_<best_fit_size) {
					found = true;
					best_fit_iter = iter;
					best_fit_size = iter->buffer_desc_.size_;
				}
			}
			ret = *best_fit_iter;
			pooled_repo_.erase(best_fit_iter);
		}
		return found;
	}

	RHIHeapMemoryBulkVulkanAllocator::RHIHeapMemoryBulkVulkanAllocator(const MemoryBulkDesc& desc) :capacity_(desc.size_), type_(desc.mem_type_) {
		auto& mem_stat = RHIDeviceMemoryStateCounter::Instance();
		mem_stat.InCreAllocCount();
		//alloc memory
		VkMemoryAllocateInfo mem_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		mem_info.pNext = nullptr;
		auto budget_size = mem_stat.GetHeapBudget(mem_stat.GetHeapIndexForMemType(type_));
		mem_info.allocationSize = std::min(capacity_, budget_size); //only here reduce memory size by budget
		mem_info.memoryTypeIndex = type_;
		auto ret = vkAllocateMemory(GetGlobalDevice(), &mem_info, g_host_alloc, &mem_bulk_);
		PCHECK(VK_SUCCESS != ret) << ": vulkan create heap memory failed";
	}


	void RHIHeapMemoryBulkVulkanAllocator::Release()
	{
		if (VK_NULL_HANDLE != mem_bulk_) {
			vkFreeMemory(GetGlobalDevice(), mem_bulk_, g_host_alloc);
		}
	}

	MemoryAllocation RHIReuseHeapMemoryBulkVulkanAllocator::Alloc(const MemoryAllocRequirements& alloc_require)
	{
		assert(alloc_require.mem_type_ == GetHeapType());
		MemoryAllocation allocation;
		Range range;
		if (FindSuitAbleFreeRange(alloc_require.size_, range)) {
			allocation.mem_ = GetBulkMemory();
			allocation.parent_ = this;
			allocation.size_ = alloc_require.size_;
			allocation.offset_ = range.offset_;
			if (range.size_ > alloc_require.size_)
			{
				range.offset_ += alloc_require.size_;
				range.size_ -= alloc_require.size_;
				free_blks_.emplace_back(range);
			}
		}
		return allocation;
	}

	void RHIReuseHeapMemoryBulkVulkanAllocator::Free(MemoryAllocation mem)
	{
		Range range{ .offset_ = mem.offset_, .size_ = mem.size_ };
		InsertAndMergeFreeRange(free_blks_, range);
	}

	void RHIReuseHeapMemoryBulkVulkanAllocator::Tick(float time)
	{
		curr_time_stamp_ += time;
		for (auto iter = used_blks_.begin(); iter != used_blks_.end(); ++iter) {
			//todo 
		}
	}

	bool RHIReuseHeapMemoryBulkVulkanAllocator::FindSuitAbleFreeRange(uint32_t size, RHIReuseHeapMemoryBulkVulkanAllocator::Range& range)
	{
		for (auto iter = free_blks_.begin(); iter != free_blks_.end(); ++iter) {
			if (iter->size_ >= size) {
				range = *iter;
				free_blks_.erase(iter);
				return true;
			}
		}
		return false;
	}
	bool RHIReuseHeapMemoryBulkVulkanAllocator::FreeRangesSanityCheck() const
	{
		for (auto iter = free_blks_.begin();;) {
			const auto offset = iter->size_ + iter->offset_;
			if ( ++iter != free_blks_.end() && offset > iter->offset_) {
				return false;
			}
		}
		return true;
	}
	bool RHIReuseHeapMemoryBulkVulkanAllocator::InsertAndMergeFreeRange(List<Range>& range_list, Range& range)
	{
		auto iter = range_list.begin();
		for (; iter != range_list.end(); ++iter) {
			if (range.offset_ < iter->offset_)
				break;
		}
		if (iter == range_list.end()) {
			range_list.emplace_back(range);
		}
		else {
			//insert range and try to merge
			auto prev_iter = iter;
			if (prev_iter != range_list.begin()) {
				--prev_iter;
				if (prev_iter->offset_ + prev_iter->size_ == range.offset_) {
					range.offset_ = prev_iter->offset_;
					range.size_ += prev_iter->size_;
					iter = range_list.erase(prev_iter);
					++iter;
				}
			}
			auto next_iter = iter;
			if (next_iter != range_list.end()) {
				if (next_iter->offset_ == range.offset_ + range.size_) {
					range.size_ += next_iter->size_;
					iter = range_list.erase(next_iter);
				}
			}
			range_list.insert(iter, range);
		}
		return true;
	}
	//https://fgiesen.wordpress.com/2010/12/14/ring-buffers-and-queues/ 
	MemoryAllocation RHIRingHeapMemoryBulkVulkanAllocator::Alloc(const MemoryAllocRequirements& alloc_require)
	{
		assert(alloc_require.mem_type_ == GetHeapType());
		const auto bulk_size = GetBulkSize();
		MemoryAllocation allocation;

		if (tail_ + bulk_size  > head_ + alloc_require.size_) {
			uint32_t padding_size = 0;
			if (alloc_require.size_ + (head_ % bulk_size) > bulk_size) {
				head_ = bulk_size;
				padding_size = bulk_size - (head_ % bulk_size);
			}
			allocation.offset_ = head_;
			allocation.parent_ = this;
			allocation.mem_ = GetBulkMemory();
			allocation.size_ = alloc_require.size_ + padding_size;
			allocation.type_ = MemoryAllocation::EType::eBulk;
			head_ += allocation.size_;
			head_ = head_ % (2 * bulk_size);
		}
		else
		{
			PLOG(ERROR) << __FUNCTION__ << ": ring buffer overflow";
		}
		return allocation;
	}

	void RHIRingHeapMemoryBulkVulkanAllocator::Free(MemoryAllocation mem)
	{
		tail_ += mem.size_;
		tail_ = tail_ % (2 * GetBulkSize());
	}

	MemoryAllocation RHILinearHeapMemoryBulkVulkanAllocator::Alloc(const MemoryAllocRequirements& alloc_require)
	{
		MemoryAllocation allocation;
		if (curr_offset_ + alloc_require.size_ < GetBulkSize())
		{
			allocation.type_ = MemoryAllocation::EType::eBulk;
			allocation.parent_ = this;
			allocation.offset_ = curr_offset_;
			allocation.size_ = alloc_require.size_;
			curr_offset_ += allocation.size_;
		}
		return allocation;
	}
	void RHILinearHeapMemoryBulkVulkanAllocator::Free(MemoryAllocation mem)
	{
		LOG(INFO) << "linear heap allocator free at once";
	}
	void ReleaseMemoryAllocation(MemoryAllocation& memory)
	{
		if (!memory.IsValid()) {
			return;
		}

		if (memory.type_ == MemoryAllocation::EType::eDedicated) {
			//delete dedicated memory
			vkFreeMemory(GetGlobalDevice(), memory.mem_, g_host_alloc);
		}
		else {
			auto allocator = memory.parent_;
			allocator->Free(memory);
		}
	}
	MemoryAllocation RHILinearHeapMemoryBulkVulkanSubAllocator::Alloc(const MemoryAllocRequirements& alloc_require)
	{
		RHILinearHeapMemoryBulkVulkanAllocator::SharedPtr allocator;
		if (auto iter = mem_heaps_.find(alloc_require.mem_type_); iter != mem_heaps_.end()) {
			auto free_blk = eastl::find(iter->second.begin(), iter->second.end(), [](auto blk) { return !blk.IsFull(); });
			if (free_blk != iter->second.end()) {
				allocator = *free_blk;
			}
		}
		
		if(allocator.get()==nullptr)
		{
			//deal with not support heap index 
			MemoryBulkDesc bulk_desc{ .size_ = RHI_PER_BLOCK_ALLOC_SIZE, .mem_type_ = alloc_require.mem_type_ };
			mem_heaps_[alloc_require.mem_type_].emplace_back(new RHILinearHeapMemoryBulkVulkanAllocator(bulk_desc));
			allocator = mem_heaps_[alloc_require.mem_type_].back();
		}
		allocator->Alloc(alloc_require);
	}
	void RHIDeviceMemoryStateCounter::Init(float heap_mem_percent)
	{
		VkPhysicalDeviceMemoryProperties2 mem_props2;
		mem_props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
		mem_budget_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
		mem_props2.pNext = &mem_budget_;
		vkGetPhysicalDeviceMemoryProperties2(GetGlobalPhyDevice(), &mem_props2);
		mem_props_ = mem_props2.memoryProperties;

		heap_infos_.resize(mem_props_.memoryHeapCount);
		for (auto n = 0; n < mem_props_.memoryHeapCount; ++n) {
			heap_infos_[n].total_size_ = mem_budget_.heapBudget[n]*heap_mem_percent;
			heap_infos_[n].used_size_ = mem_budget_.heapUsage[n];
		}

		mem_percent_ = heap_mem_percent;

		const auto& limits = RHI::Vulkan::RHIGlobalEntityVulkan::Instance()->GetVulkanDevice()->GetPhysicalDeviceProperties().limits;
		max_alloc_count_ = limits.maxMemoryAllocationCount;
	}
	void RHIDeviceMemoryStateCounter::UnInit()
	{
		//do nothing
	}
	void RHIDeviceMemoryStateCounter::Tick(float time)
	{
		VkPhysicalDeviceMemoryProperties2 mem_props2;
		mem_props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
		mem_props2.pNext = &mem_budget_;
		LOG(INFO) << "----------------Memory State----------------------";
		vkGetPhysicalDeviceMemoryProperties2(GetGlobalPhyDevice(), &mem_props2);
		for (auto n = 0; n < heap_infos_.size(); ++n) {
			heap_infos_[n].total_size_ = mem_budget_.heapBudget[n] * mem_percent_;
			heap_infos_[n].used_size_ = mem_budget_.heapUsage[n];
			heap_infos_[n].peak_size_ = std::max(heap_infos_[n].peak_size_, heap_infos_[n].used_size_);
			LOG(INFO) << "heap: " << std::setw(2) << n << " usage<" << heap_infos_[n].used_size_ << ">" <<
				" budget<" << heap_infos_[n].total_size_ << ">" << " usage ratio" << heap_infos_[n].used_size_ / mem_props_.memoryHeaps[n].size;
		}
		LOG(INFO) << "---------------------------------------------------";
	}
	VkDeviceSize RHIDeviceMemoryStateCounter::GetTotalMemorySize(bool is_gpu) const
	{
		VkDeviceSize total_size{ 0 };
		for (auto n = 0; n < mem_props_.memoryHeapCount; ++n) {
			const auto& heap_prop = mem_props_.memoryHeaps[n];
			if (is_gpu == (heap_prop.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)) {
				total_size += heap_prop.size;
			}
		}
		return total_size;
	}
	uint32_t RHIDeviceMemoryStateCounter::GetHeapIndexForMemType(uint32_t mem_type) const
	{
		return mem_props_.memoryTypes[mem_type].heapIndex;
	}
	VkDeviceSize RHIDeviceMemoryStateCounter::GetHeapBudget(uint32_t heap_index) const
	{
		return heap_infos_[heap_index].total_size_;
	}
	VkDeviceSize RHIDeviceMemoryStateCounter::GetHeapUsage(uint32_t heap_index) const
	{
		return heap_infos_[heap_index].used_size_;
	}
	VkDeviceSize RHIDeviceMemoryStateCounter::GetHeapPeakUsage(uint32_t heap_index) const
	{
		return heap_infos_[heap_index].peak_size_;
	}
	uint32_t RHIDeviceMemoryStateCounter::GetAllocCount() const
	{
		return alloc_count_;
	}
	bool RHIDeviceMemoryStateCounter::InCreAllocCount()
	{
		++alloc_count_;
		PLOG_IF(FATAL, alloc_count_ >= max_alloc_count_) << "get too many allocations";
		return alloc_count_ < max_alloc_count_;
	}
}