#include "RHI/RHISync.h"

namespace MetaInit::RHI {

	enum class EPipeline {
		eGFX,
		eAsyncCompute,
	};

	struct VulkanTransitionInfo
	{
		using Ptr = VulkanTransionInfo::Ptr;
		VkAccessFlags	src_mask_;
		VkAccessFlags	dst_mask_;
		EPipeline	src_pipeline_;
		EPipeline	dst_pipeline_;
		VkMemoryBarrier	mem_barrier_;
		SmallVector<VkBufferMemoryBarrier>	buffer_barriers_;
		SmallVector<VkImageMemoryBarrier>	image_barriers_;
		VkSemaphore	semaphore_;
		FORCE_INLINE bool IsCrossQueue() const {
			return src_pipeline_ != dst_pipeline_;
		}
	};

	static FORCE_INLINE VkAccessFlags TransRtFieldAccessToVulkanAccess() {
		return 0;
	}

	void RHICreateTransition(const Renderer::RtBarrierBatch& barrier_info, RHITransitionInfo::Ptr& trans)
	{
		auto vul_meta = trans->GetMetaData<VulkanTransitionInfo>();
		
	}

}