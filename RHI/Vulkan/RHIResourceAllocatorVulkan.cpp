#include "RHI/Vulkan/RHIResourceAllocatorVulkan.h"

namespace MetaInit::RHI::Vulkan {
	RHIResource::Ptr RHIPooledTextureAllocatorVulkan::Alloc(const RHITextureDesc& desc)
	{
		PooledTexture pooled_resource;
		if (auto ret = FindSuitAbleResource(desc, pooled_resource)) {
			return pooled_resource;
		}

		auto resource_ptr = new RHITexture(desc);
		pooled_resource.handle_ = resource_ptr;
		pooled_repo_.emplace_back(pooled_resource);
		return pooled_resource;
	}

	void RHIPooledTextureAllocatorVulkan::Free(RHIResource::Ptr resource)
	{
		PooledTexture pooled_resource{ .handle_ = dynamic_cast<RHITexture::Ptr>(resource), .last_stamp_ = curr_time_stamp_ };
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

	bool RHIPooledTextureAllocatorVulkan::FindSuitAbleResource(const RHITextureDesc& texture_desc, RHIPooledTextureAllocatorVulkan::PooledTexture& ret)
	{
		bool found = false;
		auto try_time = 2;
		const auto is_texture_convertable = [&](const RHITextureDesc& rhs) {
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
		return found;
	}

	RHIHeapMemoryBulkVulkan::RHIHeapMemoryBulkVulkan(const MemoryBulkDesc& desc) :capacity_(desc.size_) {
		//alloc memory
		VkMemoryAllocateInfo mem_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		mem_info.pNext = nullptr;
		mem_info.allocationSize = desc.size_;
		mem_info.memoryTypeIndex = 0;
		auto ret = vkAllocateMemory(GetGlobalDevice(), &mem_info, g_host_alloc, &mem_bulk_);
		PCHECK(VK_SUCCESS != ret) << ": vulkan create heap memory failed";
		free_blks_.emplace_back({nullptr, 0, capacity_});
	}

	void RHIHeapMemoryBulkVulkan::Tick(float time)
	{
		for (auto iter = used_blks_.begin(); iter != used_blks_.end(); ++iter) {
			//should free range out of time
			if (iter->first < time) {
				free_blks_.emplace_back(iter->second);
				used_blks_.erase(iter);
			}
		}
	}

	void RHIHeapMemoryBulkVulkan::Release()
	{
		if (VK_NULL_HANDLE != mem_bulk_) {
			vkFreeMemory(GetGlobalDevice(), mem_bulk_, g_host_alloc);
		}
	}

	bool RHIHeapMemoryBulkVulkan::FindSuitAbleFreeRange(uint32_t size, Range& range)
	{
		for (auto iter = free_blks_.begin(); iter != free_blks_.end(); ++iter) {
			if (iter->size_ > size) {
				range = *iter;
				free_blks_.erase(iter);
				return true;
			}
		}
		return false;
	}

	void RHITransientResourceAllocatorVulkan::Tick(float time)
	{
		for (auto& [_, bulks] : mem_heaps_) {
			for (auto& blk : bulks) {
				blk.Tick(time);
			}
		}
	}

}