#include "HAL/VulkanFrameBuffer.h"

namespace Shard {

    using namespace Primitive;
    static inline VkFramebufferCreateInfo MakeFramebufferCreateInfo(VkRenderPass pass, uint32_t width, uint32_t height, uint32_t layers,
        uint32_t attach_count, const VkImageView* pattachs)
    {
        VkFramebufferCreateInfo buffer_info{};
        memset(&buffer_info, 0, sizeof(buffer_info));
        buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        //VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT specifies that image views are not specified
        //pAttachments is a pointer to an array of VkImageView handles, each of which will be used as the
        //corresponding attachment in a render pass instance.If flags includes VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,
        //this parameter is ignored
        //buffer_info.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
        buffer_info.width = width;
        buffer_info.height = height;
        buffer_info.layers = layers;
        buffer_info.attachmentCount = attach_count;
        buffer_info.pAttachments = pattachs;
        return buffer_info;
    }

    VulkanFrameBuffer::VulkanFrameBuffer(VulkanDevice* device, VulkanRenderPass* render_pass, const VulkanFrameBuffer::Desc& desc):device_(device),pass_(render_pass)
    {
        if (!RenderPassCompatibleCheck(desc))
        {
            throw std::invalid_argument("renderpass not compatible with framebuffer");
        }
        SmallVector<VkImageView> attchments;
        for (auto n = 0; n < desc.color_attachs_.size(); ++n)
        {
            if (VK_FORMAT_UNDEFINED != desc.color_attachs_[n].image_view_.Format())
            {
                attchments.emplace_back(desc.color_attachs_[n].image_view_);
            }
        }
        //depth attachment
        if (VK_FORMAT_UNDEFINED != desc.depth_attach_.image_view_.Format())
        {
            attchments.emplace_back(desc.depth_attach_.image_view_.Format());
        }
        auto& frame_info = MakeFramebufferCreateInfo(pass_->Get(), width_, height_, layers_, attchments.size(), attchments.data());
        vkCreateFramebuffer(device_->Get(), &frame_info, g_host_alloc_vulkan, &handle_);
    }

    VulkanFrameBuffer::~VulkanFrameBuffer()
    {
        if (VK_NULL_HANDLE != handle_)
        {
            vkDestroyFramebuffer(nullptr, handle_, g_host_alloc_vulkan);
        }
    }

    const uint32_t VulkanFrameBuffer::Width() const
    {
        return width_;
    }

    const uint32_t VulkanFrameBuffer::Height() const
    {
        return height_;
    }

    const uint32_t VulkanFrameBuffer::Layers() const
    {
        return layers_;
    }

    bool VulkanFrameBuffer::RenderPassCompatibleCheck(const VulkanFrameBuffer::Desc& desc)
    {
        //todo finish check render pass and frame image view compatible
        const auto color_count = desc.color_attachs_.size();
        if (color_count != pass_->NumColorAttachments())
        {
            return false;
        }

        for (auto n = 0; n < color_count; ++n)
        {
            //check each attachment format "Two attachment references are compatible if they have
            //matching format and sample count, or are both VK_ATTACHMENT_UNUSED or the pointer
            //that would contain the reference is NULL"
            auto& color_desc = desc.color_attachs_[n];
            const VulkanImage* image = color_desc.image_view_;
            if (color_desc.image_view_.Format() != pass_->GetColorAttachMentFormat(n) ||
                 image->GetSampleCount()!= pass_->GetColorAttachMentSampleCount(n))
            {
                return false;
            }
        }

        return true;
    }

}
