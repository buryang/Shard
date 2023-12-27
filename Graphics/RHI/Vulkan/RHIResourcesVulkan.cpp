#include "RHI/Vulkan/RHIResourcesVulkan.h"
#include "RHI/Vulkan/RHIResourceAllocatorVulkan.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"
#include <typeinfo>

namespace Shard::RHI::Vulkan
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

    static inline VkImageCreateInfo TransTextureDescToVkImageCreateInfo(const RHITextureInitializer& texture_desc) {
        VkImageCreateInfo image_info;
        memset(&image_info, 0, sizeof(image_info));
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.extent = { texture_desc.layout_.width_, texture_desc.layout_.height_, texture_desc.layout_.depth_ };
        image_info.format = TransEPixFormatToVulkan(texture_desc.format_);
        return image_info;
    }

    static inline VkBufferCreateInfo TransBufferDescToVkBufferCreateInfo(const RHIBufferInitializer& buffer_desc) {
        VkBufferCreateInfo buffer_info;
        memset(&buffer_info, 0, sizeof(buffer_info));
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        return buffer_info;
    }

    void RHITextureVulkan::operator=(RHITextureVulkan&& rhs)
    {
        texture_ = rhs.texture_;
        rhs.texture_.reset();
        memory_ = std::move(rhs.memory_);
        rhs.memory_ = {};
    }
    /*vulkan resource internal realization not use input external global entity handle*/
    void RHITextureVulkan::SetUp()
    {
        assert(typeid(parent_)==typeid(RHIGlobalEntityVulkan));
        //simply check resource already setup
        if (memory_.IsValid()) {
            return;
        }
        dynamic_cast<RHIGlobalEntityVulkan::Ptr>(parent_)->SetUpTexture(this);
    }
    void RHITextureVulkan::Release()
    {
        ReleaseMemoryAllocation(memory_);
    }
    size_t RHITextureVulkan::GetOccupySize() const
    {
        return size_t(memory_.size_);
    }
    void RHITextureVulkan::SetUpHandleAlone()
    {
        const auto& texture_info = TransTextureDescToVkImageCreateInfo(desc_);
        texture_.reset(new VulkanImage(texture_info));
    }
    void RHITextureVulkan::SetUpHandleMemory(const MemoryAllocation& memory)
    {
        memory_ = memory;
        texture_->Bind(memory_.mem_, memory_.offset_);
    }
    void RHIBufferVulkan::operator=(RHIBufferVulkan&& rhs)
    {
        buffer_ = rhs.buffer_;
        rhs.buffer_.reset();
        memory_ = std::move(rhs.memory_);
        rhs.memory_ = {};
    }
    void RHIBufferVulkan::SetUp()
    {
        assert(typeid(parent_) == typeid(RHIGlobalEntityVulkan));
        //simply check resource already setup
        if (memory_.IsValid()) {
            return;
        }
        dynamic_cast<RHIGlobalEntityVulkan::Ptr>(parent_)->SetUpBuffer(this);
    }
    void RHIBufferVulkan::Release()
    {
        ReleaseMemoryAllocation(memory_);
    }
    size_t RHIBufferVulkan::GetOccupySize() const
    {
        return size_t(memory_.size_);
    }
    void* RHIBufferVulkan::MapBackMem()
    {
        return buffer_->Map();
    }
    void RHIBufferVulkan::UnMapBackMem()
    {
        buffer_->Unmap();
    }
    void RHIBufferVulkan::SetUpHandleAlone()
    {
        const auto& buffer_info = TransBufferDescToVkBufferCreateInfo(desc_);
        buffer_.reset(new VulkanBuffer(buffer_info));
    }
    void RHIBufferVulkan::SetUpHandleMemory(const MemoryAllocation& memory)
    {
        memory_ = memory;
        buffer_->Bind(memory_.mem_, memory_.offset_);
    }
}