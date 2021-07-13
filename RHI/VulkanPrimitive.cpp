#include "RHI/VulkanRHI.h"
#include "RHI/VulkanPrimitive.h"
#include "RHI/VulkanResource.h"
#include "RHI/VulkanCmdContext.h"
#include "Scene/Primitive.h"

namespace MetaInit
{
	namespace Primitive
	{

		static inline VkAccessFlags GetAccessMaskForLayout(VkImageLayout layout)
		{
			switch (layout)
			{
			#define LAYOUT_CASE(type, flags) case type: return VkAccessFlags(flags);
			LAYOUT_CASE(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT);
			LAYOUT_CASE(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
			LAYOUT_CASE(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
			LAYOUT_CASE(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT)
			LAYOUT_CASE(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
			LAYOUT_CASE(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
			LAYOUT_CASE(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_MEMORY_READ_BIT);
			LAYOUT_CASE(VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT, VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT)
			LAYOUT_CASE(VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_NONE_KHR);
			LAYOUT_CASE(VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_NONE_KHR);
			default:
				throw std::invalid_argument("not supported layout type for access");
			}
		}

		static VkPipelineStageFlags GetShaderStageMask(VkImageLayout layout)
		{
			switch (layout)
			{
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				return VK_PIPELINE_STAGE_TRANSFER_BIT;
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
				return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
				return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
				return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
				return VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;
			case VK_IMAGE_LAYOUT_GENERAL:
			case VK_IMAGE_LAYOUT_UNDEFINED:
				return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			default:
				throw std::invalid_argument("not supported layout type for shader");
			}
		}

		VulkanImage::VulkanImage(VulkanDevice::Ptr device, Image&& image, const VkImageCreateInfo& create_info):device_(device),
												Image(std::forward<Image>(image)),prop_info_(create_info), format_(create_info.format)
		{
			auto ret = vkCreateImage(nullptr, &prop_info_, nullptr, &handle_);
			assert(ret == VK_SUCCESS && "create vulkan image handle failed");

			//create memory
			VkMemoryRequirements mem_reqs;
			vkGetImageMemoryRequirements(device_->Get(), handle_, &mem_reqs);
			//FIXME
			VMAAllocation allocation;
			vmaAllocateMemoryForImage(nullptr, handle_, &mem_reqs, nullptr, &allocation.allocation_, nullptr);
			vmaBindImageMemory(nullptr, allocation.allocation_, handle_);

		}

		VulkanImage::~VulkanImage()
		{
			if (VK_NULL_HANDLE != handle_)
			{
				vkDestroyImage(device_->Get(), handle_, g_host_alloc);
				vmaFreeMemory(nullptr, vma_.allocation_);
			}
		}

		VkImage VulkanImage::Get()
		{
			return handle_;
		}

		VulkanDevice::Ptr VulkanImage::GetDevice()
		{
			return device_;
		}

		VkFormat VulkanImage::GetFormat()
		{
			return format_;
		}

		void VulkanImage::UploadData(VulkanCmdBuffer& cmd_buffer)
		{

		}

		void VulkanImage::DownloadData(VulkanCmdBuffer& cmd_buffer)
		{

		}

		void VulkanImage::ReadyForTransmit(VulkanCmdBuffer& cmd_buffer)
		{
			VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			barrier.pNext = VK_NULL_HANDLE;
			barrier.image = handle_;
			barrier.srcAccessMask = GetAccessMaskForLayout(VK_IMAGE_LAYOUT_UNDEFINED);
			barrier.dstAccessMask = GetAccessMaskForLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.subresourceRange = {};
			
			vkCmdPipelineBarrier(cmd_buffer.Get(), GetShaderStageMask(barrier.oldLayout), 
								GetShaderStageMask(barrier.newLayout), 0, 0, nullptr, 0, nullptr, 1, &barrier);
		}

		void VulkanImage::ReadyForRender(VulkanCmdBuffer& cmd_buffer)
		{
			VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			barrier.pNext = VK_NULL_HANDLE;
			barrier.image = VK_NULL_HANDLE;
			barrier.srcAccessMask = GetAccessMaskForLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			barrier.dstAccessMask = GetAccessMaskForLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.subresourceRange = {};

			vkCmdPipelineBarrier(cmd_buffer.Get(), GetShaderStageMask(barrier.oldLayout), 
									GetShaderStageMask(barrier.newLayout), 0, 0, nullptr, 0, nullptr, 1, &barrier);
		}

		VulkanImageView::VulkanImageView(VulkanImage::Ptr image, VkImageViewType view_type, uint32_t mip_level,
										 uint32_t array_layer, uint32_t n_mip_levels, uint32_t n_array_layers):image_(image),
										 device_(image->GetDevice())
		{
			assert(array_layer < image->prop_info_.arrayLayers&& mip_level < image->prop_info_.mipLevels);
			view_info_ = MakeImageViewCreateInfo(image->Get(), image->prop_info_);
			auto& sub_res = view_info_.subresourceRange;
			sub_res.baseArrayLayer = array_layer;
			sub_res.layerCount = n_array_layers < 1 ? VK_REMAINING_ARRAY_LAYERS:n_array_layers;
			sub_res.baseMipLevel = mip_level;
			sub_res.levelCount = n_mip_levels < 1 ? VK_REMAINING_MIP_LEVELS : n_mip_levels;
			auto ret = vkCreateImageView(image->GetDevice()->Get(), &view_info_, g_host_alloc, &handle_);
			assert(ret == VK_SUCCESS && "create image view failed");
		}

		VulkanImageView::~VulkanImageView()
		{
			if (VK_NULL_HANDLE != handle_)
			{
				vkDestroyImageView(device_->Get(), handle_, g_host_alloc);
			}
		}

		VkImageView	VulkanImageView::Get() 
		{
			return handle_;
		}

		VulkanSampler::VulkanSampler(Sampler&& sampler):Sampler(std::forward<Sampler>(sampler))
		{

		}

		VulkanSampler::~VulkanSampler()
		{
			if (VK_NULL_HANDLE != handle_)
			{
				vkDestroySampler(nullptr, handle_, g_host_alloc);
			}
		}

		VkFilter VulkanSampler::TransFilterMode(EFilterType filter)
		{
			switch (filter)
			{
			case EFilterType::LINEAR:
				return VkFilter::VK_FILTER_LINEAR;
			case EFilterType::NEAREST:
				return VkFilter::VK_FILTER_NEAREST;
			default:
				throw std::invalid_argument("not support filter type");
			}
		}

		VkSamplerMipmapMode VulkanSampler::TransMipmapFilterMode(EFilterType filter)
		{
			switch (filter)
			{
			case EFilterType::LINEAR:
				return VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR;
			case EFilterType::NEAREST:
				return VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST;
			default:
				throw std::invalid_argument("not support mipmap filter mode");
			}
		}
		
		VkSamplerAddressMode VulkanSampler::TransAddressMode(EAddressMode address)
		{
			switch (address)
			{
			case EAddressMode::CLAMP:
				return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			default:
				throw std::invalid_argument("not support filter address mode");
			}
		}


		VkBuffer VulkanBuffer::Get()
		{
			return handle_;
		}

		void* VulkanBuffer::Map()
		{
			if (!mapped_)
			{
				vmaMapMemory();
				mapped_ = true;
			}

			return;
		}

		VulkanBuffer& VulkanBuffer::Unmap()
		{
			return *this;
		}

		VulkanBuffer& VulkanBuffer::Update(const void* data, size_t size, size_t offset)
		{

		}
	}
}