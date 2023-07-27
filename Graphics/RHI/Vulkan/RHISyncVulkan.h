#include "RHI/RHISync.h"
#include "RHI/Vulkan/API/VulkanRHI.h"

namespace Shard::RHI::Vulkan {

	enum class EPipeline {
		eGFX,
		eAsyncCompute,
	};

	struct VulkanTransitionInfo
	{
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

	//alignment of vulkan transition info
	constexpr uint32_t ALIGN_TRANSITION_INFO_SIZE = std::alignment_of(VulkanTransitionInfo);

	static FORCE_INLINE VkAccessFlags TransRtFieldAccessToVulkanAccess() {
		return 0;
	}

	void RHICreateTransition(const Renderer::RtBarrierBatch& barrier_info, RHITransitionInfo::Ptr& trans)
	{
		auto vulkan_info = trans->GetMetaData<VulkanTransitionInfo>();
		for (auto& trans : barrier_info.transitions_) {

		}
	}

	class RHIEventVulkan : public RHIEvent
	{
	public:
		using Handle = VkEvent;
		Handle GetHandle() {
			return handle_;
		}
		RHIEventVulkan(const RHIEventInitializer& initializer);
		~RHIEventVulkan();
	private:
		Handle	handle_;
	};

}