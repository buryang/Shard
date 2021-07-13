#pragma once
#include <memory>
#include "RHI/VulkanMemAllocator.h"
#include "Scene/Primitive.h"

namespace MetaInit
{
	namespace Primitive
	{
		class VulkanImage : public Image
		{
		public:
			using Ptr = std::shared_ptr<VulkanImage>;
			explicit VulkanImage(VulkanDevice::Ptr device, Image&& image, const VkImageCreateInfo& create_info);
			~VulkanImage();
			VkImage Get();
			VkFormat GetFormat();
			VulkanDevice::Ptr GetDevice();
			void To(VulkanImage& image);
			void From(VulkanImage& image);
		private:
			void GenerateMipMap();
			void UploadData(VulkanCmdBuffer& cmd_buffer);
			void DownloadData(VulkanCmdBuffer& cmd_buffer);
			void ReadyForTransmit(VulkanCmdBuffer& cmd_buffer);
			void ReadyForRender(VulkanCmdBuffer& cmd_buffer);
		private:
			friend class VulkanImageView;
			VkImage					handle_{VK_NULL_HANDLE};
			VMAAllocation			vma_;
			VkFormat				format_;
			VkImageCreateInfo		prop_info_;
			VulkanDevice::Ptr		device_;
		};

		class VulkanImageView
		{
		public:
			VulkanImageView(VulkanImage::Ptr image, VkImageViewType view_type, uint32_t mip_level, 
								uint32_t array_layer, uint32_t n_mip_levels, uint32_t n_array_layers);
			VkImageSubresourceRange GetSubResourceRange()const { return sub_res_range_; }
			~VulkanImageView();
			VkImageView	Get();
		private:
			VulkanImage::Ptr		image_;
			VkImageView				handle_{ VK_NULL_HANDLE };
			VulkanDevice::Ptr		device_;
			VkImageViewCreateInfo	view_info_;
			VkImageSubresourceRange	sub_res_range_;
		};

		class VulkanSampler:public Sampler
		{
		public:
			explicit VulkanSampler(Sampler&& sampler);
			~VulkanSampler();
			VkSampler Get();
			static VkFilter TransFilterMode(EFilterType filter);
			static VkSamplerMipmapMode TransMipmapFilterMode(EFilterType filter);
			static VkSamplerAddressMode TransAddressMode(EAddressMode address);
		private:
			VkSampler	handle_{ VK_NULL_HANDLE };
		};


		class VulkanBuffer
		{
		public:
			explicit VulkanBuffer(VulkanDevice::Ptr device, const VkBufferCreateInfo& create_info);
			~VulkanBuffer();
			VkBuffer Get();
			void* Map();
			VulkanBuffer& Unmap();
			VulkanBuffer& Update(const void* data, size_t size, size_t offset);
		private:
			bool					mapped_{ false };
			VkBuffer				handle_{ VK_NULL_HANDLE };
			VulkanDevice::Ptr		device_;
			VMAAllocation			vma_;
			VkDeviceMemory			memory_{ VK_NULL_HANDLE };

		};
	}
}
