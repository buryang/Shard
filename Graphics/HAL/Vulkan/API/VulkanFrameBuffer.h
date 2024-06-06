#pragma once
#include "Utils/CommonUtils.h"
#include "Scene/Scene.h"
#include "VulkanRenderPass.h"

namespace Shard
{
    class MINIT_API VulkanFrameBuffer
    {
    public:
        struct _VulkanFrameBufferDesc
        {
            struct Attachment
            {
                 VulkanImageView image_view_;
            };
            SmallVector<Attachment>    color_attachs_;
            Attachment    depth_attach_;
            uint32_t    width_;
            uint32_t    height_;
            uint32_t    layers_;
            uint32_t    flags_{ 0 };
        };
        using Desc = _VulkanFrameBufferDesc;
        using Ptr = VulkanFrameBuffer*;
        VulkanFrameBuffer(VulkanDevice* device, VulkanRenderPass* render_pass, const Desc& desc);
        ~VulkanFrameBuffer();
        VkFramebuffer Get() { return handle_; }
        operator VulkanRenderPass* () { return pass_; }
        VulkanRenderPass* GetPass() { return pass_; }
        constexpr uint32_t Width()const;
        constexpr uint32_t Height()const;
        constexpr uint32_t Layers()const;
    private:
        bool RenderPassCompatibleCheck(const Desc& desc);
    private:
        VulkanDevice*            device_;
        VulkanRenderPass*        pass_;
        VkFramebuffer                handle_{ VK_NULL_HANDLE };
        uint32_t                    width_;
        uint32_t                    height_;
        uint32_t                    layers_;
        Vector<Desc::Attachment>    attachs_;
    };
}