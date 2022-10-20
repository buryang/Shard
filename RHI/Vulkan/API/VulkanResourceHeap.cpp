#include "VulkanResourceHeap.h"

void MetaInit::RHI::VulkanRHIMemoryHeap::FreeResourceMemory(RHIResource::Ptr resource)
{
	MemoryRange mem_range{.min_loc_ = resource, .max_loc_ = resource->GetOccupySize()};
	auto iter = std::find_if(free_ranges_.begin(), free_ranges_.end(), [](MemoryRange lhs, MemoryRange rhs) {
		return lhs.min_loc_ < rhs.min_loc_; });
	free_ranges_.insert(iter, std::move(mem_range));
}

void MetaInit::RHI::VulkanRHIMemoryHeap::RefreshFreeRanges()
{
	for (auto iter = free_ranges_.begin(); iter != std::next(free_ranges_.end(), -1); ) {
		auto forward_iter = std::next(iter, 1);
		if (forward_iter->min_loc_ == iter->max_loc_) {
			//FIXME merge two range but how to deal with different resource type
			forward_iter->min_loc_ = iter->min_loc_;
			iter = free_ranges_.erase(iter);
		}
		++iter;
	}
}
