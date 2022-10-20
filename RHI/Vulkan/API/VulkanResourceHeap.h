#pragma once
#include "RHI/RHIResource.h"
#include "Utils/CommonUtils.h"

/*transiant resource memory heap manager*/
namespace MetaInit::RHI {

	struct MemoryRange {
		uint32_t min_loc_{ 0 };
		uint32_t max_loc_{ 0 };
		//other
		RHIResource::Ptr entity_{ nullptr };
		bool IsFree() const {
			return entity_ == nullptr;
		}
		operator bool() const {
			return !(min_loc_ >= 0 && max_loc_ >= min_loc_);
		}
		//check whether new range overlap with old one
		bool OverlapWithOld(const MemoryRange& old) const {

		}
	};

	class VulkanRHIMemoryHeap
	{
	public:
		using Ptr = VulkanRHIMemoryHeap*;
		enum class Flags : uint8_t
		{
			eBuffer = 0x1,
			eTexture = 0x2,
			eAll = eBuffer | eTexture,
		};
		void Init(uint32_t budget, Flags flags);
		void UnInit();
		const size_t GetSize()const;
		//find suitable memory range
		bool FindSuitableRange(RHIResource::Ptr resource);
		void FreeResourceMemory(RHIResource::Ptr resource);
	private:
		void RefreshFreeRanges();
	private:
		List<MemoryRange>		free_ranges_;
		VkDeviceMemory			memory_;
	};

	class VulkanTransiantResourceHeapBulk
	{
	public:
		void Map(RHIResource::Ptr resource);
	private:
		List<VulkanRHIMemoryHeap> mem_pool_;
	};

	using TransiantResourceHeapBulk = VulkanTransiantResourceHeapBulk;
}