#include "VulkanRenderPass.h"
#include "VulkanCmdContext.h"
#include <array>

namespace
{
    constexpr auto max_render_targets = 9;
    constexpr auto max_subpass_count  = 8;
}

namespace Shard {

    static VkRenderPassCreateInfo MakeRenderPassCreateInfo(VkRenderPassCreateFlags flags, Vector<VkAttachmentDescription>& attach_descs,
        Vector<VkSubpassDescription>& subpass_descs, Vector<VkSubpassDependency>& subpass_deps)
    {
        VkRenderPassCreateInfo pass_info;
        memset(&pass_info, sizeof(VkRenderPassCreateInfo), 1);
        pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        pass_info.flags = flags;
        pass_info.pNext = VK_NULL_HANDLE;
        if (!attach_descs.empty())
        {
            pass_info.attachmentCount = attach_descs.size();
            pass_info.pAttachments = attach_descs.data();
        }
        else
        {
            pass_info.attachmentCount = 0;
            pass_info.pAttachments = VK_NULL_HANDLE;
        }

        if (!subpass_descs.empty())
        {
            pass_info.subpassCount = subpass_descs.size();
            pass_info.pSubpasses = subpass_descs.data();
        }
        else
        {
            pass_info.subpassCount = 0;
            pass_info.pSubpasses = VK_NULL_HANDLE;
        }

        if (!subpass_deps.empty())
        {
            pass_info.dependencyCount = subpass_deps.size();
            pass_info.pDependencies = subpass_deps.data();
        }
        else
        {
            pass_info.dependencyCount = 0;
            pass_info.pDependencies = VK_NULL_HANDLE;
        }
        return pass_info;

    }

    VkRenderPassBeginInfo MakeRenderPassBeginInfo(VkRenderPass render_pass, VkFramebuffer frame_buffer, VkRect2D render_area, Span<VkClearValue> clear_values)
    {
        return VkRenderPassBeginInfo();
    }

    static inline VkAttachmentLoadOp TransTargetActionToAttachLoadOp(VulkanRenderPass::Desc::ETargetAction action)
    {
        auto load_op = static_cast<VulkanRenderPass::Desc::ELoadAction>(static_cast<uint32_t>(action) >> VulkanFrameBuffer::Desc::ETargetAction::eLoadOpMask);
        switch (load_op)
        {
        case VulkanRenderPass::Desc::ELoadAction::eClear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case VulkanRenderPass::Desc::ELoadAction::eLoad:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        default:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
    }

    static inline VkAttachmentStoreOp TransTargetActionToAttachStoreOp(VulkanRenderPass::Desc::ETargetAction action)
    {
        auto save_op = static_cast<VulkanRenderPass::Desc::ESaveAction>(static_cast<uint32_t>(action) & ((2 << static_cast<uint8_t>(VulkanFrameBuffer::Desc::ETargetAction::eLoadOpMask)) - 1));
        switch (save_op)
        {
        case VulkanRenderPass::Desc::ESaveAction::eStore:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case VulkanRenderPass::Desc::ESaveAction::eResolve:
        default:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }
    }

    static inline VkSampleCountFlagBits TransSampleCountToFlagBits(uint32_t sample_count)
    {
        assert(sample_count <= 64 && (sample_count & (sample_count - 1)) == 0);
        return static_cast<VkSampleCountFlagBits>(sample_count);
    }

    VulkanRenderPass* VulkanRenderPass::Create(VulkanDevice* device, const VulkanRenderPass::Desc& desc)
    {
        auto pass_ptr = std::make_unique<VulkanRenderPass>(new VulkanRenderPass);
        pass_ptr->Init(device, desc);
        return pass_ptr;
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
        if (VK_NULL_HANDLE != handle_)
        {
            vkDestroyRenderPass(device_->Get(), handle_, g_host_alloc_vulkan);
        }
    }

    const uint32_t VulkanRenderPass::NumColorAttachments() const
    {
        return desc_.color_targets_.size();
    }

    VkFormat VulkanRenderPass::GetColorAttachMentFormat(uint32_t index) const
    {
        return VkFormat(desc_.color_targets_[index].format_);
    }

    VkSampleCountFlagBits VulkanRenderPass::GetColorAttachMentSampleCount(uint32_t index) const
    {
        return TransSampleCountToFlagBits(desc_.color_targets_[index].sample_count_);
    }

    void VulkanRenderPass::Init(VulkanDevice* device, const Desc& desc)
    {
        //1.step init attachments
        Vector<VkAttachmentDescription>    attach_descs(desc.color_targets_.size() + 1);
        uint32_t iter_counter = 0;
        for (auto& attach : desc.color_targets_)
        {
            if (attach.format_ != VK_FORMAT_UNDEFINED) //todo just copy do not understrand
            {
                VkAttachmentDescription color_attach{};
                color_attach.flags = 0;
                color_attach.format = attach.format_;
                color_attach.samples = TransSampleCountToFlagBits(attach.sample_count_);//todo FIXME
                color_attach.loadOp = TransTargetActionToAttachLoadOp(attach.action_);
                color_attach.storeOp = TransTargetActionToAttachStoreOp(attach.action_);
                //donnt care depthstencil op
                color_attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                color_attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                color_attach.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                color_attach.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attach_descs[iter_counter++] = color_attach;
            }
        }

        //depthstencil attachment
        VkAttachmentDescription depth_attach{};
        depth_attach.flags = 0;
        depth_attach.format = desc.depth_target_.format_;
        depth_attach.samples = TransSampleCountToFlagBits(desc.depth_target_.sample_count_);//todo FIXME
        depth_attach.loadOp = TransTargetActionToAttachLoadOp(desc.depth_target_.action_);
        depth_attach.storeOp = TransTargetActionToAttachStoreOp(desc.depth_target_.action_);
        depth_attach.stencilLoadOp = TransTargetActionToAttachLoadOp(desc.depth_target_.action_);
        depth_attach.stencilStoreOp = TransTargetActionToAttachStoreOp(desc.depth_target_.action_);
        depth_attach.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attach.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attach_descs[iter_counter] = depth_attach;

        //2.step init subpass
        Vector<VkSubpassDescription> subpass_descs;
        Vector<VkSubpassDependency> subpass_depends;
        
        if (!desc.subpass_descs_.empty())
        {
            for (const auto& desc : desc.subpass_descs_)
            {
                VkSubpassDescription tmp_desc{};
                tmp_desc.colorAttachmentCount = desc.color_attachs_.size();
                
            }
        }

        if (!desc.subpass_depends_.empty())
        {
            for (const auto& depend : desc.subpass_depends_)
            {
                VkSubpassDependency tmp_depend{};
            }
        }
        
        auto& pass_info = MakeRenderPassCreateInfo(0, attach_descs, subpass_descs, subpass_depends);
        vkCreateRenderPass(device->Get(), &pass_info, g_host_alloc_vulkan, &handle_);
        device_ = device;
    }

    void VulkanRenderPass::Begin(VulkanCmdBuffer& cmd, VulkanFrameBuffer& frame_buffer)
    {
        VkRenderPassBeginInfo begin_info{};
        memset(&begin_info, 0, sizeof(begin_info));
        begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        begin_info.framebuffer = frame_buffer.Get();
        begin_info.renderArea = { {0, 0}, {frame_buffer.Width(), frame_buffer.Height()} };
        begin_info.renderPass = handle_;
        vkCmdBeginRenderPass(cmd.Get(), &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    }

    void VulkanRenderPass::NextSubpass(VulkanCmdBuffer& cmd)
    {
        vkCmdNextSubpass(cmd.Get(), VK_SUBPASS_CONTENTS_INLINE);
    }

    void VulkanRenderPass::End(VulkanCmdBuffer& cmd)
    {
        vkCmdEndRenderPass(cmd.Get());
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
        if (VK_NULL_HANDLE != handle_)
        {
            vkDestroyRenderPass(device_->Get(), handle_, g_host_alloc_vulkan);
        }
    }
}
