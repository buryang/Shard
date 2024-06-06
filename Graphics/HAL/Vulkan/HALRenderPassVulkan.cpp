#include "HALRenderPassVulkan.h"

namespace Shard::HAL::Vulkan {

    static inline void MakeRenderPassCreateInfo(VkRenderPassCreateInfo& pass_info, const SmallVector<VkAttachmentDescription>& attachments, const SmallVector<VkSubpassDescription>& sub_passes,
                                                const SmallVector<VkSubpassDependency>& sub_passes_dependency)
    {
        memset(&pass_info, 0u, sizeof(pass_info));
        pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        
        if (pass_info.attachmentCount = attachments.size()) {
            pass_info.pAttachments = attachments.data();
        }
        if (pass_info.subpassCount = sub_passes.size()) {
            pass_info.pSubpasses = sub_passes.data();
        }
        if (pass_info.dependencyCount = sub_passes_dependency.size()) {
            pass_info.pDependencies = sub_passes_dependency.data();
        }
        
    }

    static inline void MakeFramebufferCreateInfo(VkFramebufferCreateInfo& fbo_info, VkRenderPass pass, const SmallVector<VkImageView>& attachments, uint32_t width, uint32_t height, uint32_t layers)
    {
        memset(&fbo_info, 0u, sizeof(fbo_info));
        fbo_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbo_info.renderPass = pass;
        fbo_info.width = width;
        fbo_info.height = height;
        fbo_info.layers = layers;
        if (fbo_info.attachmentCount = attachments.size()) {
            fbo_info.pAttachments = attachments.data();
        }
    }

    HALRenderPassVulkan::HALRenderPassVulkan(VulkanDevice::SharedPtr parent, const HALRenderPassInitializer& initializer):VulkanDeviceObject(parent)
    {
        {
            SmallVector<VkAttachmentDescription, > attachments;
            SmallVector<VkSubpassDescription> sub_passes;
            SmallVector<VkSubpassDependency> sub_passes_dependency;

            VkRenderPassCreateInfo pass_info;
            MakeRenderPassCreateInfo(pass_info, attachments, sub_passes, sub_passes_dependency);

            parent_->CreateRenderPass(&pass_info, pass_);
            assert(pass_ != VK_NULL_HANDLE);
        }

        SmallVector<VkImageView> attachments;
        VkFramebufferCreateInfo fbo_info;
        MakeFramebufferCreateInfo(fbo_info, pass_, attachments);
        parent_->CreateFrameBuffer(&fbo_info, frame_buffer_);
        assert(frame_buffer_ != VK_NULL_HANDLE);
    }

    HALRenderPassVulkan::~HALRenderPassVulkan()
    {
        if (pass_ != VK_NULL_HANDLE) {
            parent_->DestroyRenderPass(pass_);
        }
        if (frame_buffer_ != VK_NULL_HANDLE) {
            parent_->DestroyFramebuffer(frame_buffer_);
        }
    }

    HALRenderPassRepoVulkan::Handle HALRenderPassRepoVulkan::Alloc(const HALRenderPassInitializer& initializer)
    {
        auto handle = Parent::Alloc();
        if (handle.Index() > pass_storage_.size()) {
            pass_storage_.emplace_back({nullptr, initializer});
        }
        else {
            new(&pass_storage_[handle.Index()])HALRenderPassVulkan(nullptr, initializer);
        }
        return handle;
    }

    void HALRenderPassRepoVulkan::Free(HALRenderPassRepoVulkan::Handle handle)
    {
        if (Parent::IsValid(handle)) {
            pass_storage_[handle.Index()].~HALRenderPassVulkan();
            Parent::Free(handle);
        }
    }

    HALRenderPassVulkan* HALRenderPassRepoVulkan::Get(Handle handle)
    {
        if (Parent::IsValid(handle)) {
            return &pass_storage_[handle.Index()];
        }
        return nullptr;
    }

    void HALRenderPassRepoVulkan::Clear()
    {
        pass_storage_.clear();
    }

}
