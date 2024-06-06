#include "HAL/HALSync.h"
#include "HAL/Vulkan/API/VulkanHAL.h"

namespace Shard::HAL::Vulkan {

    enum class EPipeline {
        eGFX,
        eAsyncCompute,
    };

    struct VulkanTransitionInfo
    {
        VkAccessFlags    src_mask_;
        VkAccessFlags    dst_mask_;
        EPipeline    src_pipeline_;
        EPipeline    dst_pipeline_;
        VkMemoryBarrier    mem_barrier_;
        SmallVector<VkBufferMemoryBarrier>   buffer_barriers_;
        SmallVector<VkImageMemoryBarrier>    image_barriers_;
        VkSemaphore    semaphore_;
        FORCE_INLINE bool IsCrossQueue() const {
            return src_pipeline_ != dst_pipeline_;
        }
    };

    //alignment of vulkan transition info
    constexpr auto ALIGN_TRANSITION_INFO_SIZE = std::alignment_of_v<VulkanTransitionInfo>;

    static FORCE_INLINE VkAccessFlags TransFieldAccessToVulkan(EAccessFlags field_access) {
        switch (field_access)
        {
        case EAccessFlags::eReadOnly:
            return VK_ACCESS_SHADER_READ_BIT; 
        case EAccessFlags::eIndexBuffer:
            return VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
        case EAccessFlags::eDSVRead:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        case EAccessFlags::eDSVWrite:
            return VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case EAccessFlags::eTransferSrc:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case EAccessFlags::eTransferDst:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case EAccessFlags::eVertexBuffer:
            return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        default:
            assert(0);
            LOG(ERROR) << "render field access flag not support";
        }
    }

    class HALEventVulkan final: public HALEvent, public VulkanDeviceObject
    {
    public:
        void* GetHandle()override {
            return handle_;
        }
        HALEventVulkan(const HALEventInitializer& initializer);
        ~HALEventVulkan();
    private:
        VkEvent handle_;
    };

    class HALFenceVulkan final : public HALFence, public VulkanDeviceObject
    {

    };

    class HALSemaphoreVulkan final : public HALSemaphore, public VulkanDeviceObject
    {
    public:
       explicit HALSemaphoreVulkan(VulkanDevice::SharedPtr parent, const String& name);
       ~HALSemaphoreVulkan();
       void Signal(uint64_t expected_value) override;
       void Wait(uint64_t expected_value) override;
    protected:
       uint64_t GetSemaphoreCounterValueImpl()const override;
    private:
        VkSemaphore handle_;
    };

}