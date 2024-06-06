#pragma once
#include "HAL/Vulkan/API/VulkanHAL.h"
#include "HAL/HALRenderPass.h"

namespace Shard::HAL::Vulkan
{
    class HALRenderPassVulkan : public HALRenderPass, public VulkanDeviceObject
    {
    public:
        HALRenderPassVulkan(VulkanDevice::SharedPtr parent, const HALRenderPassInitializer& initializer);
        ~HALRenderPassVulkan();
        void* GetHandle()override {
            return pass_;
        }
    private:
        VkRenderPass    pass_{ VK_NULL_HANDLE };
        VkFramebuffer   frame_buffer_{ VK_NULL_HANDLE };
    };
    
    class HALRenderPassRepoVulkan : public HALRenderPassRepo
    {
    public:
        using HALRenderPassRepo::Parent;
        using HALRenderPassRepo::Handle;
        using PassAllocator = std::remove_pointer_t<decltype(g_hal_allocator)>::rebind<HALRenderPassVulkan>::other;
        HALRenderPassRepoVulkan(PassAllocator allocator = *g_hal_allocator) :pass_storage_(allocator) {}
        Handle Alloc(const HALRenderPassInitializer& initializer) override;
        void Free(Handle handle) override;
        void Clear();
        HALRenderPassVulkan* Get(Handle handle);
    private:
        Vector<HALRenderPassVulkan, PassAllocator> pass_storage_; 
    };
}
