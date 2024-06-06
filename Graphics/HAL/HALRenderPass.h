#pragma once
#include "Utils/Handle.h"

namespace Shard::HAL
{
    //only vulkan has subpass
    struct HALRenderSubpassInitializer
    {
        Span<uint8_t>   color_attachments_;
        Span<uint8_t>   input_attachments_; 
        Span<uint8_t>   resolve_attachments_;
        uint8_t depth_stencil_attachment_{ ~0u };
    };
    
    struct HALRenderPassInitializer
    {
        uvec4   roi_area_;
        uint8_t base_layer_{ 0u };
        uint8_t num_layers_{ 1u };
        Span<Hande<HALTexture>> color_attachments_:
        Handle<HALTexture>  depth_stencil_attachment_;
        Span<HALRenderSubpassInitializer> sub_passes_;
    };

    class HALRenderPass;
    class HALRenderPassRepo : public Utils::HandleManager<HALRenderPass>
    {
    public:
        using Parent = Utils::HandleManager<HALRenderPass>;
        using Handle = Utils::HandleManager<HALRenderPass>::Handle;
        virtual Handle Alloc(const HALRenderPassInitializer& initializer) = 0;
        virtual void Free(Handle) = 0;
        virtual ~HALRenderPassRepo() {}
    };

    class HALRenderPass
    {
    public:
        using Handle = HALRenderPassRepo::Handle;
        virtual ~HALRenderPass() = 0;
    };

}
