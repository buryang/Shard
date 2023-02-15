#include "RHI/Vulkan/RHIResourcesVulkan.h"
#include "RHI/Vulkan/RHIResourceAllocatorVulkan.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"
#include <typeinfo>

namespace MetaInit::RHI::Vulkan
{
	static inline VkFormat ConvertEPixFormatToVulkan(EPixFormat format) {
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
			//todo
			return 0;
		case EPixFormat::eRGB5A1Unorm:
			return VK_FORMAT_
		//to do add others
		case EPixFormat::eUnkown:
		default:
			return VK_FORMAT_UNDEFINED;
		}
	}

	static inline VkImageCreateInfo ConvertTextureDescToVkImageCreateInfo(const RHITextureDesc& texture_desc) {
		VkImageCreateInfo image_info;
		memset(&image_info, 0, sizeof(image_info));
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.extent = { texture_desc.layout_.width_, texture_desc.layout_.height_, texture_desc.layout_.depth_ };
		image_info.format = ConvertEPixFormatToVulkan(texture_desc.format_);
		return image_info;
	}

	static inline VkBufferCreateInfo ConvertBufferDescToVkBufferCreateInfo(const RHIBufferDesc& buffer_desc) {
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
		const auto& texture_info = ConvertTextureDescToVkImageCreateInfo(desc_);
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
		const auto& buffer_info = ConvertBufferDescToVkBufferCreateInfo(desc_);
		buffer_.reset(new VulkanBuffer(buffer_info));
	}
	void RHIBufferVulkan::SetUpHandleMemory(const MemoryAllocation& memory)
	{
		memory_ = memory;
		buffer_->Bind(memory_.mem_, memory_.offset_);
	}
}