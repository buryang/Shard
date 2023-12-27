#pragma once
#include "RHI/Vulkan/API/VulkanRHI.h"
#include "RHI/Vulkan/API/VulkanCmdContext.h"
#include "RHI/RHIResources.h"

namespace Shard
{
        /*resource state*/
        enum class EResourceState : uint32_t
        {
                eUndefined,
                ePreInitialized,
                eCommon,
                eVertexBuffer,
                eIndexBuffer,
                eUnorderedAccess,
                eDepthStencil,
                eShaderResource,
                ePresent,
                eCopyDst,
                eCopySrc,
                eResolveSrc,
                eResolveDst,
                eIndirectArgs,
                eRenderTarget,
        };

        class VulkanBuffer;
        class VulkanImageView;
        class VulkanImage
        {
        public:
                using Ptr = VulkanImage*;
                using SharedPtr = eastl::shared_ptr<VulkanImage>;
                using Handle = VkImage;
                VulkanImage() = default;
                explicit VulkanImage(const VkImageCreateInfo& create_info);
                void Init(const VkImageCreateInfo& create_info);
                ~VulkanImage();
                Handle Get();
                FORCE_INLINE VkFormat GetFormat()const { return format_; }
                FORCE_INLINE VkSampleCountFlags GetSampleCount()const { return sample_count_; }
                FORCE_INLINE EResourceState GetState()const { return state_; }
                FORCE_INLINE uint32_t GetLevels()const { return mips_; }
                FORCE_INLINE uint32_t GetLayers()const { return layers_; }
                VulkanImage& SetState(EResourceState new_state) { state_ = new_state; return *this; }
                VulkanImage& Clear(VkClearValue value, const VkImageSubresourceRange& region);
                VulkanImage& Bind(VkDeviceMemory memory, VkDeviceSize offset);
                void CopyTo(VulkanCmdBuffer::SharedPtr cmd_buffer, SharedPtr image);
                void CopyTo(VulkanCmdBuffer::SharedPtr cmd_buffer, VulkanBuffer::SharedPtr buffer);
        private:
                void GenerateMipMap(VulkanCmdBuffer::SharedPtr cmd_buffer);
                void UploadData(VulkanCmdBuffer::SharedPtr cmd_buffer);
                void DownloadData(VulkanCmdBuffer::SharedPtr cmd_buffer);
        private:
                friend class VulkanImageView;
                VkImage                                        handle_{VK_NULL_HANDLE};
                VkExtent3D                                dims_;
                VkFormat                                format_;
                EResourceState                        state_{ EResourceState::eUndefined };
                uint32_t                                mips_;
                uint32_t                                layers_;
                VkSampleCountFlags                sample_count_;
        };

        class VulkanImageView
        {
        public:
                VulkanImageView() = default;
                VulkanImageView(VulkanImage::Ptr image, VkImageViewType view_type, VkFormat format, uint32_t level,
                                                        uint32_t layer, uint32_t mip_levels, uint32_t array_layers);
                void Init((VulkanImage::Ptr image, VkImageViewType view_type, VkFormat format, uint32_t level,
                        uint32_t layer, uint32_t mip_levels, uint32_t array_layers);
                VkImageSubresourceRange GetSubResourceRange()const { return sub_res_range_; }
                ~VulkanImageView();
                VkImageView        Get();
                VkFormat Format()const { return format_; }
                VkImageViewType ViewType()const { return view_type_; }
                operator VulkanImage::Ptr(){ return image_; }
        private:
                VulkanImage::Ptr                image_;
                VkImageView                                handle_{ VK_NULL_HANDLE };
                VkFormat                                format_;
                VkImageViewType                        view_type_;
                VkImageSubresourceRange        sub_res_range_;
        };

        class VulkanImageSamplerFactory
        {
        public:
                using Key = VkSamplerCreateInfo;
                using Val = VkSampler;
                static VulkanImageSamplerFactory& Instance() {
                        static VulkanImageSamplerFactory factory;
                        return factory;
                }
                ~VulkanImageSamplerFactory() {
                        for (auto& [_, sampler] : sampler_repo_) {
                                vkDestroySampler(nullptr, sampler, g_host_alloc);
                        }
                }
                Val GetOrCreateSampler(VulkanDevice::SharedPtr device, const Key& desc) {
                        if (auto iter = sampler_repo_.find(desc); iter != sampler_repo_.end()) {
                                return iter->second;
                        }
                        Val filter = VK_NULL_HANDLE;
                        auto ret = vkCreateSampler(device->Get(), &desc, g_host_alloc, &filter);
                        CHECK(VK_SUCCESS == ret) << __FUNCTION__ << " : create sampler failed" << std::endl;
                        sampler_repo_[desc] = filter;
                        return filter;
                }
        private:
                VulkanImageSamplerFactory() = default;
                DISALLOW_COPY_AND_ASSIGN(VulkanImageSamplerFactory);
        private:
                Map<Key, Val>        sampler_repo_;
        };


        class VulkanBuffer
        {
        public:
                using Handle = VkBuffer;
                using SharedPtr = eastl ::shared_ptr<VulkanBuffer>;
                VulkanBuffer() = default;
                explicit VulkanBuffer(const VkBufferCreateInfo& create_info);
                void Init(const VkBufferCreateInfo& create_info);
                ~VulkanBuffer();
                Handle Get()const;
                VkDeviceSize GetSize()const;
                EResourceState GetState()const;
                VulkanBuffer& SetState(EResourceState new_state);
                void* Map() const;
                VulkanBuffer& Unmap();
                VulkanBuffer& Update(const uint8_t* data, size_t size, size_t offset);
                VulkanBuffer& Flush()const;
                VulkanBuffer& Clear(uint32_t data);
                VulkanBuffer& Bind(VkDeviceMemory memory, VkDeviceSize offset);
                void CopyTo(VulkanCmdBuffer::SharedPtr cmd_buffer, VulkanImage::SharedPtr image);
                void CopyTo(VulkanCmdBuffer::SharedPtr cmd_buffer, SharedPtr buffer);
                bool IsStage()const;
        private:
                friend class VulkanBufferView;
                friend class VulkanImage;
                Handle        handle_{ VK_NULL_HANDLE };
                void*        mapped_data_{ nullptr };
                //VkBuffer        stage_buffer_{ VK_NULL_HANDLE };
                VkBufferCreateInfo        prop_info_;
                EResourceState        state_{ EResourceState::eUndefined };
        };

        class VulkanBufferView
        {
        public:
                using Handle = VkBufferView;
                VulkanBufferView() = default;
                explicit VulkanBufferView(VulkanBuffer::SharedPtr buffer, VkBufferViewCreateFlags flags,
                                                                        VkFormat format, uint32_t offset, uint32_t range);
                void Init(VulkanBuffer::SharedPtr buffer, VkBufferViewCreateFlags flags,
                        VkFormat format, uint32_t offset, uint32_t range);
                ~VulkanBufferView();
                Handle Get();
                operator VulkanBuffer::SharedPtr() { return buffer_; }
        private:
                VulkanBuffer::SharedPtr        buffer_;
                Handle        handle_{ VK_NULL_HANDLE };
        };

        class VulkanVertexAttributes
        {
        public:
                enum ETopology
                {
                        ePoint,
                        eLine,
                        eTriangle,
                        ePatch,
                };
                VulkanVertexAttributes();
                VulkanBuffer::SharedPtr GetVertex();
                VulkanBuffer::SharedPtr GetIndex();
        private:
                VulkanBuffer::SharedPtr        vertex_;
                VulkanBuffer::SharedPtr        index_;
        };

        VkImageCreateInfo MakeImageCreateInfo(VkImageCreateFlags flags, VkFormat format);
        VkImageViewCreateInfo MakeImageViewCreateInfo(VkImageViewCreateFlags flags, VkImage image, const VkImageCreateInfo& imageInfo);
        VkBufferCreateInfo MakeBufferCreateInfo(VkBufferCreateFlags flags, uint32_t size);
        VkBufferViewCreateInfo MakeBufferViewCreateInfo(VkBufferViewCreateFlags flags, VkBuffer buffer,
                VkFormat format, VkDeviceSize offset, VkDeviceSize range);
        VkSamplerCreateInfo MakeSamplerCreateInfo(VkSamplerCreateFlags flags, VkFilter mag_filter, VkFilter min_filter, VkSamplerMipmapMode mipmap_mode,
                VkSamplerAddressMode address_modeu, VkSamplerAddressMode address_modev, VkSamplerAddressMode address_modew);
}
