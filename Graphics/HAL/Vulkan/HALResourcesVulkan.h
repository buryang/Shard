#pragma once

#include "HAL/HALResources.h"
#include "HAL/Vulkan/API/VulkanResources.h"
#include "HAL/Vulkan/HALGlobalEntityVulkan.h"

namespace Shard::HAL::Vulkan
{
    class HALTextureVulkan final : public HALTexture, public VulkanDeviceObject
    {
    public:
        using Ptr = HALTextureVulkan*;
        using SharedPtr = eastl::shared_ptr<HALTextureVulkan>;
        HALTextureVulkan(HALGlobalEntityVulkan* parent, const HALTextureInitializer& desc) :HALTexture(parent, desc) {}
        HALTextureVulkan(HALTextureVulkan&& other);
        void operator=(HALTextureVulkan&& other);
        void SetUp() override;
        void Release() override;
        size_t GetOccupySize() const override;
        //only inital vulkan texture handle
        void SetUpHandleAlone();
        void SetUpHandleMemory(const MemoryAllocation& memory);
        FORCE_INLINE VulkanImage::SharedPtr GetImpl() {
            return texture_;
        }
        bool IsTilingOptimal()const;
        ~HALTextureVulkan() { Release(); }
    private:
        friend class HALGlobalEntityVulkan;
        VulkanImage::SharedPtr    texture_;
        HALAllocation   memory_;
        VkPipelineStageFlags    stage_{ VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM };
    };

    class HALTextureViewVulkan final : public HALTextureView, public VulkanDeviceObject
    {

    };

    class HALBufferVulkan final : public HALBuffer, public VulkanDeviceObject
    {
    public:
        using Ptr = HALBufferVulkan*;
        using SharedPtr = eastl::shared_ptr<HALBufferVulkan>;
        HALBufferVulkan(HALGlobalEntityVulkan* parent, const HALBufferInitializer& desc);
        void operator=(HALBufferVulkan&& rhs);
        void SetUp() override;
        void Release() override;
        size_t GetOccupySize() const override;
        //only initial vulkan buffer handle
        void SetUpHandleAlone();
        void SetUpHandleMemory(const HALAllocation& memory);
        void* GetHandle() override {
            return buffer_;
        }
        ~HALBufferVulkan() { Release(); }
    private:
        friend class HALGlobalEntityVulkan;
        VulkanBuffer::SharedPtr buffer_;
        HALAllocation   memory_;
        VkPipelineStageFlags    stage_{ VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM };
    };

    class HALBufferViewVulkan final : public HALBufferView, public VulkanDeviceObject
    {
    public:

    };

}