#pragma once
#include <memory>
#include "RHI/VulkanFrameGraph.h"
#include "Scene/Primitive.h"


namespace MetaInit
{
	
	class VulkanFrameContextGraph;
	using RenderGraph = VulkanFrameContextGraph;
	namespace Primitive
	{
		class VulkanBuffer;
		class VulkanImage
		{
		public:
			using Ptr = std::shared_ptr<VulkanImage>;
			explicit VulkanImage(RenderGraph::Ptr graph, Image&& image, const VkImageCreateInfo& create_info, bool gen_mips=false);
			explicit VulkanImage(RenderGraph::Ptr graph, VulkanBuffer::Ptr buffer, bool gen_mips=false);
			explicit VulkanImage(RenderGraph::Ptr graph, const VkImageCreateInfo& create_info);
			~VulkanImage();
			VkImage Get();
			VkFormat GetFormat()const;
			uint32_t GetSampleCount()const;
			VulkanImage& Clear(VkClearValue value, const VkImageSubresourceRange& region);
			void To(VulkanImage& image);
			VulkanImage& From(VulkanImage& image);
		private:
			void GenerateMipMap(VulkanCmdBuffer& cmd_buffer);
			void UploadData(VulkanCmdBuffer& cmd_buffer);
			void DownloadData(VulkanCmdBuffer& cmd_buffer);
			//need fix layout for each subresource not resource
			void ReadyForTransmit(VulkanCmdBuffer& cmd_buffer); 
			void ReadyForRender(VulkanCmdBuffer& cmd_buffer);
			static VkImageType TransImageType(EImageType);
		private:
			friend class VulkanImageView;
			VkImage					handle_{VK_NULL_HANDLE};
			VkExtent3D				size_;
			VkFormat				format_;
			VkImageCreateInfo		prop_info_;
			RenderGraph::Ptr		graph_;
			VmaAllocation			vma_data_;
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
			VkImageViewCreateInfo	view_info_;
			VkImageSubresourceRange	sub_res_range_;
		};

		class VulkanSampler
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
			VkSamplerMipmapMode		mip_mode_{ VK_SAMPLER_MIPMAP_MODE_NEAREST };
			VkSamplerAddressMode	address_mode_{ VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE };
		};


		class VulkanBuffer
		{
		public:
			using Ptr = std::shared_ptr<VulkanBuffer>;
			explicit VulkanBuffer(RenderGraph::Ptr graph, const VkBufferCreateInfo& create_info);
			~VulkanBuffer();
			VkBuffer Get();
			void* Map();
			VulkanBuffer& Unmap();
			VulkanBuffer& Update(const uint8_t* data, size_t size, size_t offset);
			VulkanBuffer& Flush()const;
			VulkanBuffer& Clear(uint32_t data);
		private:
			friend class VulkanBufferView;
			friend class VulkanImage;
			bool					mapped_{ false };
			VkBuffer				handle_{ VK_NULL_HANDLE };
			VkBufferCreateInfo		prop_info_;
			RenderGraph::Ptr		graph_;
			VmaAllocation			vma_data_;
		};

		class VulkanBufferView
		{
		public:
			explicit VulkanBufferView(VulkanBuffer::Ptr buffer, VkBufferViewCreateFlags flags,
										VkFormat format, uint32_t offset, uint32_t range);
			~VulkanBufferView();
			VkBufferView Get();
		private:
			VulkanBuffer::Ptr	buffer_;
			VkBufferView		handle_{ VK_NULL_HANDLE };
		};
	}
}
