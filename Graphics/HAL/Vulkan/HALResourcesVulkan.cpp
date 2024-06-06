#include "HAL/Vulkan/HALResourcesVulkan.h"
#include "HAL/Vulkan/HALResourceAllocatorVulkan.h"
#include "HAL/Vulkan/HALGlobalEntityVulkan.h"
#include <typeinfo>

namespace Shard::HAL::Vulkan
{
    static inline VkFormat TransEPixFormatToVulkan(EPixFormat format) {
        switch (format) {
        case EPixFormat::eR8Unorm:
            return VK_FORMAT_R8_UNORM;
        case EPixFormat::eR8Snorm:
            return VK_FORMAT_R8_SNORM;
        case EPixFormat::eR16Unorm:
            return VK_FORMAT_R16_UNORM;
        case EPixFormat::eR16Snorm:
            return VK_FORMAT_R16_SNORM;
        case EPixFormat::eRG8Unorm:
            return VK_FORMAT_R8G8_UNORM;
        case EPixFormat::eRG8Snorm:
            return VK_FORMAT_R8G8_SNORM;
        case EPixFormat::eRG16Unorm:
            return VK_FORMAT_R16G16_UNORM;
        case EPixFormat::eRG16Snorm:
            return VK_FORMAT_R16G16_SNORM;
        case EPixFormat::eR24UnormX8:
            return VK_FORMAT_X8_D24_UNORM_PACK32;
        case EPixFormat::eRGB5A1Unorm:
            return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
        case EPixFormat::eR32FloatX32:
        case EPixFormat::eR32Float:
            return VK_FORMAT_R32_SFLOAT;
        case EPixFormat::eBC1Unorm:
            return VK_FORMAT_BC1_RGB_UNORM_BLOCK;//todo
        case EPixFormat::eBC1UnormSrgb:
            return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
        case EPixFormat::eBC2Unorm:
            return VK_FORMAT_BC2_UNORM_BLOCK;
        case EPixFormat::eBC2UnormSrgb:
            return VK_FORMAT_BC2_SRGB_BLOCK;
        case EPixFormat::eBC3Unorm:
            return VK_FORMAT_BC3_UNORM_BLOCK;
        case EPixFormat::eBC3UnormSrgb:
            return VK_FORMAT_BC3_SRGB_BLOCK;
        case EPixFormat::eBC4Unorm:
            return VK_FORMAT_BC4_UNORM_BLOCK;
        case EPixFormat::eBC4Snorm:
            return VK_FORMAT_BC4_SNORM_BLOCK;
        case EPixFormat::eBC5Unorm:
            return VK_FORMAT_BC5_UNORM_BLOCK;
        case EPixFormat::eBC5Snorm:
            return VK_FORMAT_BC5_SNORM_BLOCK;
        case EPixFormat::eBC6HS16:
            return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case EPixFormat::eBC6HU16:
            return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case EPixFormat::eBC7Unorm:
            return VK_FORMAT_BC7_UNORM_BLOCK;
        case EPixFormat::eBC7UnormSrgb:
            return VK_FORMAT_BC7_SRGB_BLOCK;
        //to do add others
        case EPixFormat::eUnkown:
        default:
            return VK_FORMAT_UNDEFINED;
        }
    }
    static inline VkImageAspectFlags TransEPixFormatToVulkanAspect(EPixFormat format){
        switch (format)
        {
        case EPixFormat::eUnkown:
            return 0u;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
        case EPixFormat::eD32FloatS8X24:
        case EPixFormat::eD24UnormS8:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        case EPixFormat::eD32Float:
        case EPixFormat::eD16Unorm:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case xx:
            return VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    static inline void MakeVulkanImageCreateInfo(const HALTextureInitializer& texture_desc, VkImageCreateInfo& image_info) {
        memset(&image_info, 0, sizeof(image_info));
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.extent = { texture_desc.dims_.width_, texture_desc.dims_.height_, texture_desc.dims_.depth_ };
        image_info.format = TransEPixFormatToVulkan(texture_desc.format_);
    }

    static inline void MakeVulkanBufferCreateInfo(const HALBufferInitializer& buffer_desc, VkBufferCreateInfo& buffer_info) {
        memset(&buffer_info, 0, sizeof(buffer_info));
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    static inline void MakeVulkanImageViewCreateInfo(HALTextureVulkan* texture, const HALTextureViewInitializer& view_desc, VkImageViewCreateInfo& view_info) {
        memset(&view_info, 0u, sizeof(view_info));
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = texture->GetImpl();
        view_info.viewType = texture->xx;
        view_info.subresourceRange.aspectMask = TransEPixFormatToVulkanAspect(view_desc.format_);
        view_info.subresourceRange.baseMipLevel = view_desc.region_.base_mip_;
        view_info.subresourceRange.levelCount = view_desc.region_.mips_;
        view_info.subresourceRange.baseArrayLayer = view_desc.region_.base_layer_;
        view_info.subresourceRange.layerCount = view_desc.region_.layers_;
        view_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    }
    static inline void MkaeVulkanBufferViewCreateInfo(HALBufferVulkan* buffer, VkBufferViewCreateInfo& view_info) {
        memset(&view_info, 0u, sizeof(view_info));
        view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        view_info.buffer = xx;
        view_info.format = xx;
        view_info.offset = xx;
        view_info.range = xx;
        view_info.flags = xx;
    }

    void HALTextureVulkan::operator=(HALTextureVulkan&& rhs)
    {
        texture_ = rhs.texture_;
        rhs.texture_.reset();
        memory_ = std::move(rhs.memory_);
        rhs.memory_ = {};
    }
    /*vulkan resource internal realization not use input external global entity handle*/
    void HALTextureVulkan::SetUp()
    {
        assert(typeid(parent_)==typeid(HALGlobalEntityVulkan));
        //simply check resource already setup
        if (memory_.IsValid()) {
            return;
        }
        dynamic_cast<HALGlobalEntityVulkan*>(parent_)->SetUpTexture(this);
    }
    void HALTextureVulkan::Release()
    {
        ReleaseMemoryAllocation(memory_);
        for (auto& [_, image_view] : texture_views_) {
            parent_->DestroyImageView(image_view);
        }
        texture_views_.clear();
    }
    size_t HALTextureVulkan::GetOccupySize() const
    {
        return size_t(memory_.size_);
    }
    VkImageView HALTextureVulkan::GetOrCreateView(HALTextureViewInitializer& view_desc)
    {
        assert(texture_views_.size() < xxx &&"");
        const auto view_hash = uint32_t(view_desc.Hash());
        if (auto iter = texture_views_.find(view_hash); iter != texture_views_.end()) {
            return iter->second;
        }
        //create image view
        VkImageView image_view;
        VkImageViewCreateInfo image_view_info;
        parent_->CreateImageView(&image_view_info, image_view);
        texture_views_.insert({ view_hash, image_view });
        return image_view;
    }
    void HALTextureVulkan::SetUpHandleAlone()
    {
        const auto& texture_info = MakeVulkanImageCreateInfo(desc_);
        texture_.reset(new VulkanImage(texture_info));
    }
    void HALTextureVulkan::SetUpHandleMemory(const MemoryAllocation& memory)
    {
        memory_ = memory;
        texture_->Bind(memory_.mem_, memory_.offset_);
    }
    bool HALTextureVulkan::IsTilingOptimal() const
    {
        return !!texture_->GetTiling();
    }
    HALBufferVulkan::HALBufferVulkan(HALGlobalEntityVulkan* parent, const HALBufferInitializer& desc):HALBuffer(parent, desc)
    {
        //initial uav counter
        if (desc.uav_counter_) {
            HALBufferInitializer uav_counter_initalizer{ .size_ = 4, .access_ = EAccessFlags::eNone, .uav_counter_ = 0u}; //todo
            uav_counter_ = new HALBufferVulkan(parent, uav_counter_initalizer);
        }
    }
    void HALBufferVulkan::operator=(HALBufferVulkan&& rhs)
    {
        buffer_ = rhs.buffer_;
        rhs.buffer_.reset();
        memory_ = std::move(rhs.memory_);
        rhs.memory_ = {};
    }
    void HALBufferVulkan::SetUp()
    {
        assert(typeid(parent_) == typeid(HALGlobalEntityVulkan));
        //simply check resource already setup
        if (memory_.IsValid()) {
            return;
        }
        dynamic_cast<HALGlobalEntityVulkan*>(parent_)->SetUpBuffer(this);
    }
    void HALBufferVulkan::Release()
    {
        ReleaseMemoryAllocation(memory_);
    }
    size_t HALBufferVulkan::GetOccupySize() const
    {
        return size_t(memory_.size_);
    }

    void HALBufferVulkan::SetUpHandleAlone()
    {
        const auto& buffer_info = MakeVulkanBufferCreateInfo(desc_);
        buffer_.reset(new VulkanBuffer(buffer_info));
    }
    void HALBufferVulkan::SetUpHandleMemory(const MemoryAllocation& memory)
    {
        memory_ = memory;
        buffer_->Bind(memory_.mem_, memory_.offset_);
    }
}