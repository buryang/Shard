#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"
#define VK_USE_PLATFORM_WIN32_KHR
#include "volk/volk.h"
#include "glfw/glfw3.h"

namespace MetaInit
{
	extern VkAllocationCallbacks* g_host_alloc;
	static constexpr uint32_t MAX_QUEUE_COUNT = 16;

	class VulkanInstance
	{
	public:
		using SharedPtr = std::shared_ptr<VulkanInstance>;
		using Handle = VkInstance;
		static SharedPtr Create();
		Handle Get() { return handle_; }
		~VulkanInstance() { vkDestroyInstance(handle_, g_host_alloc); }
	private:
		VkInstance	handle_{ VK_NULL_HANDLE };
	};

	//todo alloc queue, and do swapchain init check
	class VulkanQueue;
	class VulkanCmdPoolManager;
	class VulkanDevice : public std::enable_shared_from_this<VulkanDevice>
	{
	public:
		using SharedPtr = std::shared_ptr<VulkanDevice>;
		using Handle = VkDevice;
		using PhyHandle = VkPhysicalDevice;
		static SharedPtr Create(VulkanInstance::SharedPtr instance);
		FORCE_INLINE Handle Get() { return handle_; }
		FORCE_INLINE PhyHandle GetPhyHandle() { return back_end_; }
		FORCE_INLINE VkPipelineCache GetPipelineCache() { return pipeline_cache_; }
		FORCE_INLINE const VkPhysicalDeviceProperties& GetPhysicalDeviceProperties() const { return back_end_properties_; }
		FORCE_INLINE const VkPhysicalDeviceDescriptorIndexingProperties& GetPhysicalDeviceDescriptorIndexingProperties()const { return back_end_descrptor_indexing_properties_; }
		//function to deal with command buffer logic
		VulkanCmdPoolManager& GetCmdPoolManager() { return pool_manager_; }
		VkSampleCountFlags GetMaxUsableSampleCount()const;
		VkSurfaceCapabilitiesKHR GetSurfaceSupportInfo()const;
		VkFormatProperties GetFormatProperty(VkFormat format)const;
		void GetSwapchainSupportInfo()const;
		bool IsFormatSupported(VkFormat format)const;
		void WaitIdle()const;
		//function to deal with device relative pipeline cache
		void LoadPipelineCache(const String& cache_path);
		void RestorePipelineCache(const String& cache_path);
		~VulkanDevice();
	private:
		VulkanDevice() = default;
		DISALLOW_COPY_AND_ASSIGN(VulkanDevice);
		static VkPhysicalDevice SelectSuitAbleDevice(VulkanInstance::SharedPtr instance, const Span<const char*> extensions);
	private:
		VkDevice	handle_{ VK_NULL_HANDLE };
		VkPhysicalDevice back_end_{ VK_NULL_HANDLE };
		VkPhysicalDeviceProperties back_end_properties_;
		VkPhysicalDeviceDescriptorIndexingProperties back_end_descrptor_indexing_properties_;
		VkPipelineCache	pipeline_cache_{ VK_NULL_HANDLE };//todo 
		VulkanCmdPoolManager	pool_manager_;
	};

	class VulkanCmdBuffer;
	class VulkanQueue
	{
	public:
		enum class EQueueType : uint32_t
		{
			eUnkown = 0x0,
			eGraphics = 0x1,
			eCompute = 0x2,
			eTransfer = 0x4,
			eNonCompute = eGraphics | eTransfer,
			eNonGFX = eCompute | eTransfer, //compute and transfer queue 
			eAllIn = eGraphics | eCompute | eTransfer,
		};
		using SharedPtr = std::shared_ptr<VulkanQueue>;
		static SharedPtr Create(VulkanQueue::EQueueType queue_type);
		static FORCE_INLINE void RegistFamily(EQueueType queue_type, uint32_t family_index) {
			queue_families_[queue_type] = family_index;
		}
		void Submit(VulkanCmdBuffer::SharedPtr cmd_buffers, const Span<VkSemaphore>& wait_semaphores, const Span<VkSemaphore>& signal_semaphores, VkFence fence=VK_NULL_HANDLE);
		void Submit(const VkPresentInfoKHR& present_info);
		void WaitIdle()const;
		VkQueue Get()const { return handle_; };
		uint32_t GetFamilyIndex()const { return family_index_; }
		uint32_t GetIndex()const { return index_; }
	private:
		explicit VulkanQueue(uint32_t family_index, uint32_t index);
		DISALLOW_COPY_AND_ASSIGN(VulkanQueue);
	private:
		static Map<EQueueType, uint32_t>	queue_families_;
		VkQueue	handle_{ VK_NULL_HANDLE };
		uint32_t	family_index_{ -1 };
		uint32_t	index_{ -1 };
	};

	static inline VkDevice GetGlobalDevice() {
		return RHI::Vulkan::RHIGlobalEntityVulkan::Instance()->GetVulkanDevice()->Get();
	}

	static inline VkPhysicalDevice GetGlobalPhyDevice() {
		return RHI::Vulkan::RHIGlobalEntityVulkan::Instance()->GetVulkanDevice()->GetPhyHandle();
	}

	static inline const VkAllocationCallbacks* GetGlobalAllocationCallBacks() {
		return RHI::Vulkan::RHIGlobalEntityVulkan::Instance()->GetVulkanCallBack();
	}

}

