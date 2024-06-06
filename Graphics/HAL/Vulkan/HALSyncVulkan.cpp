#include "HALSyncVulkan.h"

namespace Shard::HAL::Vulkan {

    static inline void MakeSemaphoreSignalInfo(VkSemaphoreSignalInfo& signal_info, VkSemaphore semaphore, uint64_t value) {
        memset(&signal_info, 0u, sizeof(signal_info));
        signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signal_info.semaphore = semaphore;
        signal_info.value = value;
    }

    static inline void MakeSemaphoreWaitInfo(VkSemaphoreWaitInfo& wait_info, VkSemaphore& semaphore, uint64_t& value) {
        memset(&wait_info, 0u, sizeof(wait_info));
        wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wait_info.semaphoreCount = 1;
        wait_info.pSemaphores = &semaphore;
        wait_info.pValues = &value;
    }

    void HALCreateTransition(const Render::BarrierBatch& barrier_batch, HALTransitionInfo& trans_info)
    {
        auto vulkan_info = trans_info.GetMetaData<VulkanTransitionInfo>();
        for (auto& trans : barrier_batch.transitions_) {
            
        }
        for (auto& barrier : barrier_batch.barriers_) {

        }
    }

    void HALBeginTransition(HALCommandContext* cmd, const HALTransitionInfo* trans)
    {

    }

    void HALEndTransition(HALCommandContext* cmd, const HALTransitionInfo* trans)
    {

    }

    HALEventVulkan::HALEventVulkan(const HALEventInitializer& initializer):HALEvent(initializer), VulkanDeviceObject(nullptr)
    {
        VkEventCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
            .pNext = nullptr,
            .flags = initializer.flags_
        };
        
        auto ret = parent_->CreateEvents(&create_info, handle_);
        PCHECK(ret) << "failed to create vulkan event";
    }

    HALEventVulkan::~HALEventVulkan()
    {
        if (handle_ != VK_NULL_HANDLE) {
            parent_->DestroyEvents(handle_);
        }
    }

    HALSemaphoreVulkan::HALSemaphoreVulkan(VulkanDevice::SharedPtr parent, const String& name):HALSemaphore(name), VulkanDeviceObject(parent)
    {
        VkSemaphoreCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0u
        };
        //default to be timeline semaphore
        VkSemaphoreTypeCreateInfo timeline_create_info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext = nullptr,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue = 0u,
        };
        VSChainPushFront(&create_info, &timeline_create_info);
        auto ret = parent_->CreateSemaphore(&create_info, handle_);
        PCHECK(ret) << "failed to create vulkan semaphore";
    }
    HALSemaphoreVulkan::~HALSemaphoreVulkan()
    {
        if (handle_ != VK_NULL_HANDLE) {
            parent_->DestroySemaphore(handle_);
        }
    }
    void HALSemaphoreVulkan::Signal(uint64_t expected_value)
    {
        VkSemaphoreSignalInfo signal_info;
        MakeSemaphoreSignalInfo(signal_info, handle_, expected_value);
        parent_->SignalSemaphore(signal_info);
    }
    void HALSemaphoreVulkan::Wait(uint64_t expected_value)
    {
        VkSemaphoreWaitInfo wait_info;
        MakeSemaphoreWaitInfo(wait_info, handle_, expected_value_); //todo
        parent_->WaitSemaphore(wait_info);
    }
    uint64_t HALSemaphoreVulkan::GetSemaphoreCounterValueImpl() const
    {
        uint64_t curr_value{ 0u };
        parent_->GetSemaphoreCounterValue(handle_, curr_value);
        return curr_value;
    }
}
