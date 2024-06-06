#pragma once
#include "HAL/HALQuery.h"
#include "HAL/Vulkan/API/VulkanHAL.h"

#ifdef DEVELOP_DEBUG_TOOLS
namespace Shard::HAL::Vulkan
{
    class HALQueryPoolVulkan final : public HALQueryPool, public VulkanDeviceObject
    {
    public:
        HALQueryPoolVulkan(EType type, uint32_t count, uint32_t queue_index);
        ~HALQueryPoolVulkan();
        FORCE_INLINE void* GetHandle()override{
            return handle_;
        }
        void Reset()override;
        void WaitReadBack()override;
        uint64_t GetTimeStampFrequency() const override;
    private:
        VkQueryPool handle_{ VK_NULL_HANDLE };
        uint64_t    time_freq_{ 0u };
    };
}
#endif