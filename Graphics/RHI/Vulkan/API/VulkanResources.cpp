#include "RHI/Vulkan/API/VulkanRHI.h"
#include "RHI/Vulkan/API/VulkanCmdContext.h"
#include "RHI/Vulkan/API/VulkanResources.h"
#include "RHI/Vulkan/API/VulkanDescriptor.h"
#include "Scene/Primitive.h"

namespace Shard
{
	VkImageCreateInfo MakeImageCreateInfo(VkImageCreateFlags flags, VkFormat format)
	{
		VkImageCreateInfo image_info{};
		memset(&image_info, 0, sizeof(image_info));
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.flags = flags;
		image_info.format = format;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		/*pQueueFamilyIndices is a list of queue families that will access this image (ignored if sharingModeis not VK_SHARING_MODE_CONCURRENT).*/
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		//not support cubemap now
		image_info.arrayLayers = 1;
		image_info.mipLevels = 1;
		return image_info;
	}

	static inline VkImageViewType GetImageViewType(VkImageType image_type)
	{
		switch (image_type)
		{
		case VK_IMAGE_TYPE_1D:
			return VK_IMAGE_VIEW_TYPE_1D;
		case VK_IMAGE_TYPE_2D:
			return VK_IMAGE_VIEW_TYPE_2D;
		default:
			PLOG(ERROR) << "not supported image view type";
		}
	}

	VkImageViewCreateInfo MakeImageViewCreateInfo(VkImageViewCreateFlags flags, VkImage image, VkImageViewType view_type)
	{
		VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		view_info.flags = flags;
		view_info.pNext = VK_NULL_HANDLE;
		view_info.viewType = view_type;
		return view_info;
	}

	VkBufferCreateInfo MakeBufferCreateInfo(VkBufferCreateFlags flags, uint32_t size)
	{
		VkBufferCreateInfo buffer_info{};
		memset(&buffer_info, 0, sizeof(buffer_info));
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.flags = flags;
		buffer_info.size = size;
		/*pQueueFamilyIndices is a list of queue families that will access this image (ignored if sharingModeis not VK_SHARING_MODE_CONCURRENT).*/
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		return buffer_info;
	}

	VkBufferViewCreateInfo MakeBufferViewCreateInfo(VkBufferViewCreateFlags flags, VkBuffer buffer,
		VkFormat format, VkDeviceSize offset, VkDeviceSize range)
	{
		VkBufferViewCreateInfo view_info{};
		memset(&view_info, 0, sizeof(view_info));
		view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
		view_info.flags = flags;
		view_info.buffer = buffer;
		view_info.format = format;
		view_info.offset = offset;
		view_info.range = range;
		return view_info;
	}

	VkSamplerCreateInfo MakeSamplerCreateInfo(VkSamplerCreateFlags flags, VkFilter mag_filter, VkFilter min_filter,
		VkSamplerMipmapMode mipmap_mode, VkSamplerAddressMode address_modeu,
		VkSamplerAddressMode address_modev, VkSamplerAddressMode address_modew)
	{
		VkSamplerCreateInfo sample_info{};
		memset(&sample_info, 0, sizeof(sample_info));
		sample_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sample_info.flags = flags;
		sample_info.magFilter = mag_filter;
		sample_info.minFilter = min_filter;
		sample_info.addressModeU = address_modeu;
		sample_info.addressModeV = address_modev;
		sample_info.addressModeW = address_modew;
		return sample_info;
	}

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
		LAYOUT_CASE(VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT, VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT);
		LAYOUT_CASE(VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_NONE_KHR);
		LAYOUT_CASE(VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_NONE_KHR);
		#undef LAYOUT_CASE
		default:
			PLOG(ERROR) << "not supported layout type for access";
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
			PLOG(ERROR) << "not supported layout type for shader";
		}
	}

	VulkanImage::VulkanImage(VulkanDevice::SharedPtr device, Image&& image, const VkImageCreateInfo& create_info):device_(device), 
								size_({image.size_.x, image.size_.y, image.size_.z}), prop_info_(create_info), format_(create_info.format)
	{
		prop_info_.imageType = TransImageType(image.type_);
		prop_info_.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
		auto ret = vkCreateImage(nullptr, &prop_info_, nullptr, &handle_);
		assert(ret == VK_SUCCESS && "create vulkan image handle failed");

		//create memory
		VkMemoryRequirements mem_reqs;
		auto& device = graph_->GetDevice();
		auto& allocator = graph_->GetMemAllocator();
		vkGetImageMemoryRequirements(device->Get(), handle_, &mem_reqs);
		//FIXME
		VMAAllocation allocation;
		vmaAllocateMemoryForImage(allocator->Get(), handle_, &mem_reqs, nullptr, &allocation.allocation_, nullptr);
		vmaBindImageMemory(allocator->Get(), allocation.allocation_, handle_);

		UploadData();
	}

	VulkanImage::VulkanImage(VulkanDevice::Ptr device, VulkanBuffer::SharedPtr buffer):device_(device)
	{
		//todo
		ReadyForTransmit();
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource = {};
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {};
		vkCmdCopyBufferToImage(device_->Get(), buffer->Get(), handle_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	VulkanImage::VulkanImage(VulkanDevice::Ptr device, const VkImageCreateInfo& create_info):device_(device)
	{
		auto ret = vkCreateImage(device->Get(), &create_info, g_host_alloc, &handle_);
		assert(ret == VK_SUCCESS && "create vulkan image faild");
	}

	VulkanImage::VulkanImage(const VkImageCreateInfo& create_info)
	{
	}

	VulkanImage::~VulkanImage()
	{
		if (VK_NULL_HANDLE != handle_)
		{
			vkDestroyImage(device_->Get(), handle_, g_host_alloc);
			vmaFreeMemory(nullptr, vma_data_.allocation_);
		}
	}

	VulkanImage& VulkanImage::Clear(VkClearValue value, const VkImageSubresourceRange& region)
	{
		if (region.aspectMask&VK_IMAGE_ASPECT_COLOR_BIT) 
		{
			const auto&color_value = value.color;
			vkCmdClearColorImage(device_->Get(), handle_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color_value, 1, &region);
		}
		else if(region.aspectMask&(VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT))
		{
			const auto& depth_value = value.depthStencil;
			vkCmdClearDepthStencilImage(device_->Get(), handle_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &depth_value, 1, &region);
		}
		else
		{
			throw std::invalid_argument("invalid region aspect mask");
		}
		return *this;
	}

	VulkanImage& VulkanImage::Bind(VkDeviceMemory memory, VkDeviceSize offset)
	{
		vkBindImageMemory(GetGlobalDevice(), handle_, memory, offset);
		return *this;
	}

	void VulkanImage::GenerateMipMap(VulkanCmdBuffer& cmd_buffer)
	{
		uint32_t mip_width = 1024, mip_height = 1024;
		VkImageMemoryBarrier barrier{};
		barrier.image = handle_;
		barrier.subresourceRange = {};
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		for (auto level = 1; level < prop_info_.mipLevels; ++level)
		{
			mip_width = std::max((uint32_t)1, mip_width >> 1);
			mip_height = std::max((uint32_t)1, mip_height >> 1);
			//barrier 
			barrier.subresourceRange.baseMipLevel = level - 1;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			vkCmdPipelineBarrier(cmd_buffer.Get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
									0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);
			/*blit logic:https://vulkan-tutorial.com/Generating_Mipmaps */
			VkImageBlit blit_region{};
			blit_region.srcOffsets[0] = { 0, 0, 0 };
			blit_region.srcOffsets[1] = { mip_width, mip_height, 1 };
			blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit_region.srcSubresource.mipLevel = level - 1;
			blit_region.srcSubresource.baseArrayLayer = 0;
			blit_region.srcSubresource.layerCount = 1;
			blit_region.dstOffsets[0] = { 0, 0, 0 };
			blit_region.dstOffsets[1] = { std::max((uint32_t)1, mip_width >> 1), std::max((uint32_t)1, mip_height >> 1), 1 };
			blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit_region.dstSubresource.mipLevel = level;
			blit_region.dstSubresource.baseArrayLayer = 0;
			blit_region.dstSubresource.layerCount = 1;
			/*the layout of the "source image subresources" for the blit*/
			vkCmdBlitImage(cmd_buffer.Get(), handle_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, handle_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit_region, VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
								0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);
		}

		barrier.subresourceRange.baseMipLevel = prop_info_.mipLevels - 1;
		vkCmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 
							0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &barrier);

	}

	void VulkanImage::UploadData(VulkanCmdBuffer& cmd_buffer)
	{
		ReadyForTransmit(cmd_buffer);
		auto data = raw_.data();
		auto buffer_info = MakeBufferCreateInfo(1, 1);
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		VulkanBuffer buffer(device_, buffer_info);
		//layer count == 0, level count = 0
		SmallVector<VkBufferImageCopy> regions(1);
		region[0].bufferOffset = 0;
		region[0].bufferRowLength = 0; //tightly packed, no padding
		region[0].bufferImageHeight = 0; //tightly packed, no padding
		{
			region[0].imageSubresource = {};
		}
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { size_.x, size_.y, size_.z }; //todo 
		vkCmdCopyBufferToImage(cmd_buffer.Get(), buffer.Get(), handle_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, region.data());
		ReadyForRender(cmd_buffer);

		if (prop_info_.mipLevels > 1)
		{
			GenerateMipMap(cmd_buffer);
		}
	}

	void VulkanImage::DownloadData(VulkanCmdBuffer& cmd_buffer)
	{
		throw std::logic_error("not implemented download image data");
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
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
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
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange = {};

		vkCmdPipelineBarrier(cmd_buffer.Get(), GetShaderStageMask(barrier.oldLayout), 
								GetShaderStageMask(barrier.newLayout), 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	VkImageType VulkanImage::TransImageType(EImageType type)
	{
		switch (type)
		{
		case EImageType::TEXTURE_1D:
			return VK_IMAGE_TYPE_1D;
		case EImageType::TEXTURE_2D:
			return VK_IMAGE_TYPE_2D;
		case EImageType::TEXTURE_3D:
			return VK_IMAGE_TYPE_3D;
		default:
			throw std::invalid_argument("not supported image tpye");
		}
	}

	VulkanImageView::VulkanImageView(VulkanImage::Ptr image, VkImageViewType view_type, VkFormat format, uint32_t mip_level,
										uint32_t array_layer, uint32_t n_mip_levels, uint32_t n_array_layers):image_(image),view_type_(view_type), format_(format)
										
	{
		auto image_vs_view_comp = [=](void)->bool {
			const auto& image_info = image->prop_info_;
			switch (view_type)
			{
			case VK_IMAGE_VIEW_TYPE_1D:
				return (image_info.imageType == VK_IMAGE_TYPE_1D && image_info.arrayLayers == 1);
			case VK_IMAGE_VIEW_TYPE_2D:
				return (image_info.imageType == VK_IMAGE_TYPE_2D && image_info.arrayLayers == 1);
			case VK_IMAGE_VIEW_TYPE_3D:
				return (image_info.imageType == VK_IMAGE_TYPE_3D && image_info.arrayLayers == 1);
			case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
				return (image_info.imageType == VK_IMAGE_TYPE_1D && image_info.arrayLayers > 1);
			case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
				return (image_info.imageType == VK_IMAGE_TYPE_2D && image_info.arrayLayers > 1);
			/*
			case VK_IMAGE_VIEW_TYPE_CUBE:
			case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
				return ((image_info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) && image_info.imageType == VK_IMAGE_TYPE_2D);
			default:
				return false;
			*/
			}
		};

		if(!image_vs_view_comp())
		{
			throw std::invalid_argument("in-compability view and image parameters");
		}
		assert(array_layer < image->prop_info_.arrayLayers&& mip_level < image->prop_info_.mipLevels);
		auto view_type = GetImageViewType(image->prop_info_.imageType);
		auto& view_info = MakeImageViewCreateInfo(static_cast<VkImageViewCreateFlags>(0), image->Get(), view_type);
		auto& sub_res = view_info.subresourceRange;
		sub_res.baseArrayLayer = array_layer;
		sub_res.layerCount = n_array_layers < 1 ? VK_REMAINING_ARRAY_LAYERS : n_array_layers;
		sub_res.baseMipLevel = mip_level;
		sub_res.levelCount = n_mip_levels < 1 ? VK_REMAINING_MIP_LEVELS : n_mip_levels;
		sub_res_range_ = sub_res;
		auto ret = vkCreateImageView(image->graph_->GetDevice()->Get(), view_info, g_host_alloc, &handle_);
		assert(ret == VK_SUCCESS && "create image view failed");
	}

	VulkanImageView::~VulkanImageView()
	{
		if (VK_NULL_HANDLE != handle_)
		{
			vkDestroyImageView(image_->GetDevice()->Get(), handle_, g_host_alloc);
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

	VulkanBuffer::VulkanBuffer(VulkanDevice::Ptr device, const VkBufferCreateInfo& create_info):prop_info_(create_info)
	{
		auto ret = vkCreateBuffer(device->Get(), &create_info, g_host_alloc, &handle_);
		assert(ret == VK_SUCCESS);
	}

	VkBuffer VulkanBuffer::Get()const
	{
		return handle_;
	}

	VkDeviceSize VulkanBuffer::GetSize()const
	{
		return prop_info_.size;
	}

	void* VulkanBuffer::Map()const
	{
		if (!mapped_)
		{
			void* ptr = nullptr;
			vmaMapMemory(graph_->GetMemeAllocator().Get(), allocation_, &ptr);
			mapped_ = true;
			return ptr;
		}
		return nullptr;
	}

	VulkanBuffer& VulkanBuffer::Unmap()
	{
		vmaUnmapMemory(graph_->GetMemeAllocator().Get(), allocation_);
		return *this;
	}

	VulkanBuffer& VulkanBuffer::Update(const uint8_t* data, size_t size, size_t offset)
	{
		auto ptr = Map();
		std::memcpy(ptr, data + offset, size);
		Unmap();
	}

	VulkanBuffer& VulkanBuffer::Flush()	const
	{
		if (mapped_)
		{
			vmaFlushAllocation(graph_->GetMemeAllocator().Get(), vma_data_, 0, GetSize());
		}
		return *this;
	}

	VulkanBuffer& VulkanBuffer::Clear(uint32_t data)
	{
		assert(VK_BUFFER_USAGE_TRANSFER_DST_BIT & prop_info_.usage);
		vkCmdFillBuffer(graph_->GetMemeAllocator().Get(), handle_, 0, VK_WHOLE_SIZE, data);
		return *this;
	}

	VulkanBuffer& VulkanBuffer::Bind(VkDeviceMemory memory, VkDeviceSize offset)
	{
		vkBindBufferMemory(GetGlobalDevice(), handle_, memory, offset);
		return *this;
	}

	bool VulkanBuffer::IsStage()const
	{
		return prop_info_.usage & 0; //todo
	}

	VulkanBufferView::VulkanBufferView(VulkanBuffer::SharedPtr buffer, VkBufferViewCreateFlags flags, VkFormat format,
										uint32_t offset, uint32_t range):buffer_(buffer)
	{

		auto& buffer_info = buffer->prop_info_;
		if ((buffer_info.usage & (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT))||buffer_info.size < offset+range)
		{
			assert(0 && "buffer not support view");
		}

		auto view_info = MakeBufferViewCreateInfo(flags, buffer->Get(), format, offset, range);
		auto ret = vkCreateBufferView(GetGlobalDevice(), &view_info, g_host_alloc, &handle_);
	}

	VulkanBufferView::Handle VulkanBufferView::Get()
	{
		return handle_;
	}

	VulkanBufferView::~VulkanBufferView()
	{
		if (VK_NULL_HANDLE != handle_)
		{
			vkDestroyBufferView(GetGlobalDevice(), handle_, g_host_alloc);
		}
	}

}