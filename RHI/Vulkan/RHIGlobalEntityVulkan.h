#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHIGlobalEntity.h"
#include "RHI/Vulkan/API/VulkanRHI.h"
#include "RHI/Vulkan/RHIResourceAllocatorVulkan.h"

namespace MetaInit::RHI {
	namespace Vulkan
	{
		class RHIGlobalEntityVulkan : public RHIGlobalEntity
		{
		public:
			using Ptr = RHIGlobalEntityVulkan*;
			static Ptr Instance();
			void Init() override;
			void UnInit() override;
			void CreateSampler() override;
			void CreateViewPoint() override;
			RHIShaderLibraryInterface::Ptr GetOrCreateShaderLibrary() override;
			RHIPipelineStateObjectLibraryInterface::Ptr GetOrCreatePSOLibrary() override;
			RHIResourceBindlessHeap::Ptr GetOrCreateResourceBindlessHeap() override;
			RHIResource::Ptr CreateConstBuffer(const RHIBufferInitializer& desc) override;
			RHIResource::Ptr CreateStructedBuffer(const RHIBufferInitializer& desc) override;
			RHIResource::Ptr CreateTexture(const RHITextureInitializer& desc) override;
			RHIResource::Ptr CreateUAV(const RHITextureUAVInitializer& desc) override;
			RHIResource::Ptr CreateSRV(const RHITextureSRVInitializer& desc) override;
			//RHIResource::Ptr CreateRayTracingAccelerateStruct() override;
			RHICommandContext::Ptr CreateCommandBuffer() override;
			//calculate resource video memory size
			size_t ComputeMemorySize(RHIResource::Ptr res) const override;
			void SetViewPoint() override;
			void ResizeViewPoint() override;
			void Execute(Span<RHICommandContext::Ptr> cmd_buffers) override;
			bool SetUpTexture(RHITextureVulkan::Ptr texture);
			bool SetUpBuffer(RHIBufferVulkan::Ptr buffer);
			//special functions for vulkan
			FORCE_INLINE VulkanInstance::SharedPtr GetVulkanInstance() {
				assert(nullptr != instance_.get());
				return instance_;
			}
			FORCE_INLINE VulkanDevice::SharedPtr GetVulkanDevice() {
				assert(nullptr != device_.get());
				return device_;
			}
			FORCE_INLINE bool SetVulkanCallBack(VkAllocationCallbacks* clk) {
				alloc_clk_ = clk;
			}
			FORCE_INLINE const VkAllocationCallbacks* GetVulkanCallBack() const {
				return alloc_clk_;
			}
		protected:

		private:
			RHIGlobalEntityVulkan();
			void InitInstance();
			void InitGlobalAllocationCallBacks();
		private:
			VulkanInstance::SharedPtr	instance_;
			VulkanDevice::SharedPtr	device_;
			//viewport 

			//memory resource manager
			RHIPooledTextureAllocatorVulkan	pooled_repo_;
			RHITransientResourceAllocatorVulkan::SharedPtr transient_repo_;
			
			//memory alloc wrapper for vulkan
			VkAllocationCallbacks* alloc_clk_{ nullptr };
		}; 
	}
}
