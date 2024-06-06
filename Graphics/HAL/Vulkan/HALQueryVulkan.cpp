#include "HALQueryVulkan.h"

#ifdef DEVELOP_DEBUG_TOOLS
namespace Shard::HAL::Vulkan
{
    static inline VkQueryType TransPoolTypeToVulkan(HALQueryPool::EType type) {
        switch (type) {
        case HALQueryPool::EType::eOcclusion:
        case HALQueryPool::EType::eBinaryOcclusion:
            return VK_QUERY_TYPE_OCCLUSION;
        case HALQueryPool::EType::ePipelineStats:
            return VK_QUERY_TYPE_PIPELINE_STATISTICS;
        case HALQueryPool::EType::eTimeStamp:
            return VK_QUERY_TYPE_TIMESTAMP;
        default:
            assert(0 && "query type vulkan not support");
        }
    }

    //If VK_QUERY_RESULT_64_BIT is set, results and, if returned, availability or status values for all queries are written as an array of 64-bit values.
    //If VK_QUERY_RESULT_WITH_AVAILABILITY_BIT or VK_QUERY_RESULT_WITH_STATUS_BIT_KHR is set, the layout of data in the buffer is a (result,availability) 
    //or (result,status) pair for each query returned, and stride is the stride between each pair.
    //We donnot use pair here, so stride is sizeof(uint64_t)
    HALQueryPoolVulkan::HALQueryPoolVulkan(HALQueryPool::EType type, uint32_t count, uint32_t queue_index) :HALQueryPool(type, count, sizeof(uint64_t), queue_index), VulkanDeviceObject()
    {
        if (type == HALQueryPool::EType::eTimeStamp) {
            VkPhysicalDeviceProperties device_props;
            parent_->GetPhysicalDeviceProperties(device_props);
            if (device_props.limits.timestampPeriod == 0u) {
                LOG(FATAL) << "the device donnot support timestamp query");
            } 
            if(!device_props.limits.timestampComputeAndGraphics) {
                VkQueueFamilyProperties queue_props; //todo
                if (queue_props.timestampValidBits == 0u) {
                    LOG(FATAL) << "the select queue does not support timestamp query";
                }
            }    
            const auto period_seconds = device_props.limits.timestampPeriod * 1e-9;
            time_freq_ = uint64_t(1.f / period_seconds + 0.5.f);
        }
        VkQueryPoolCreateInfo pool_info;
        memset(&pool_info, 0u, sizeof(pool_info));
        pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        pool_info.queryCount = size;
        pool_info.queryType = TransPoolTypeToVulkan(type);
        pool_info.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_FLAG_BITS_MAX_ENUM;
        parent_->CreateQueryPool(&pool_info, &handle_);
    }
    HALQueryPoolVulkan::~HALQueryPoolVulkan()
    {
        if (handle_ != VK_NULL_HANDLE) {
            parent_->DestroyQueryPool(handle_);
        }
    }
    void HALQueryPoolVulkan::Reset()
    {
        parent_->ResetQueryPool(handle_, 0u, size);
    }
    void HALQueryPoolVulkan::WaitReadBack()
    {
        HALQueryPool::ReadBack();
        parent_->GetQueryPoolResults(handle_, 0u, size_, sizeof(uint64_t) * size_, nullptr);
    }

    uint64_t HALQueryPoolVulkan::GetTimeStampFrequency() const
    {
        return time_freq_;
    }
}
#endif
