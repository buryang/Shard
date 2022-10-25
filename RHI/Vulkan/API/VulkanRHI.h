#pragma once
#include "Utils/CommonUtils.h"
#define VK_USE_PLATFORM_WIN32_KHR
#include "volk/volk.h"
#include "glfw/glfw3.h"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace MetaInit
{
	extern VkAllocationCallbacks* g_host_alloc;

	VkDeviceCreateInfo MakeDeviceCreateInfo(VkDeviceCreateFlags flags, const Vector<VkDeviceQueueCreateInfo>& queue_infos,
		const Vector<const char*>& ext_infos, const Vector<const char*>& layer_infos,
		const Vector<VkPhysicalDeviceFeatures>& device_features);
	VkInstanceCreateInfo MakeInstanceCreateInfo(VkInstanceCreateFlags flags, const VkApplicationInfo& app_info,
		const Vector<const char*> ext_infos, const Vector<const char*>& layer_infos);

	class MINIT_API VulkanInstance
	{
	public:
		using SharedPtr = std::shared_ptr<VulkanInstance>;
		using Handle = VkInstance;
		static SharedPtr Create(const VkInstanceCreateInfo& params);
		Handle Get() { return handle_; }
		~VulkanInstance() { vkDestroyInstance(handle_, g_host_alloc); }
	private:
		VkInstance	handle_{ VK_NULL_HANDLE };
		Vector<VkExtensionProperties>	extensions_;
		VkPipelineCache	pipeline_cache_{ VK_NULL_HANDLE };
	};

	//todo alloc queue, and do swapchain init check
	class VulkanVMAWrapper;
	class VulkanQueue;
	class VulkanCmdPoolManager;
	class MINIT_API VulkanDevice : public std::enable_shared_from_this<VulkanDevice>
	{
	public:
		enum class EQueueType :uint32_t
		{
			eGraphics,
			eCompute,
			eTransfer,
			eAllIn,
		};
		using Ptr = std::shared_ptr<VulkanDevice>;
		static Ptr Create(VulkanInstance::Ptr instance, const VkDeviceCreateInfo& device_info);
		using QueryCallback = std::function<bool(VkPhysicalDevice device, uint32_t family_index)>;
		VkDevice Get() { return handle_; }
		VkPhysicalDevice GetPhy() { return phy_devices_; }
		VulkanQueue::Ptr GetQueue(EQueueType queue_type);
		VulkanQueue::Ptr QueryQueue(QueryCallback query);
		VkPipelineCache GetPipelineCache() { return pipeline_cache_; }
		//function to deal with command buffer logic
		VulkanCmdPoolManager& GetCmdPoolManager() { return pool_manager_; }
		void ReleaseQueue(VulkanQueue::Ptr queue);
		VkSampleCountFlags GetMaxUsableSampleCount()const;
		uint32_t GetMaxColorTargetCount()const;
		uint32_t GetMaxPushConstantLimit()const;
		uint32_t GetMaxVertexInputAttributes()const;
		uint32_t GetMaxVertexInputBinds()const;
		VkDeviceSize GetMinUniformBufferOffsetAlignment()const;
		VkSurfaceCapabilitiesKHR GetSurfaceSupportInfo()const;
		VkFormatProperties GetFormatProperty(VkFormat format)const;
		void GetSwapchainSupportInfo()const;
		bool IsFormatSupported(VkFormat format)const;
		//VkPhysicalDevice operator[](uint32_t index);
		void WaitIdle()const;
		//function to deal with device relative pipeline cache
		void LoadPipelineCache(const std::string& cache_path);
		void RestorePipelineCache(const std::string& cache_path);
		~VulkanDevice();
	private:
		VulkanDevice() = default;
		DISALLOW_COPY_AND_ASSIGN(VulkanDevice);
		void Init();
	private:
		VkDevice	handle_{ VK_NULL_HANDLE };
		VkPhysicalDevice	phy_devices_{ VK_NULL_HANDLE };
		VkPhysicalDeviceProperties	device_prop_;
		VkDeviceCreateInfo	device_info_;
		VkPipelineCache	pipeline_cache_{ VK_NULL_HANDLE };//todo 
		VulkanCmdPoolManager	pool_manager_;
		//pair<family_index, handle>
		Map<EQueueType, Vector<uint32_t> >	queue_families_;
		Vector<Vector<int> >	queue_inuse_;
		std::mutex	mutex_;
	};

	class VulkanCmdBuffer;
	class VulkanQueue
	{
	public:
		using Ptr = std::shared_ptr<VulkanQueue>;
		VulkanQueue() = default;
		explicit VulkanQueue(VulkanDevice::Ptr device, uint32_t family_index, uint32_t index);
		DISALLOW_COPY_AND_ASSIGN(VulkanQueue);
		void Submit(Span<VulkanCmdBuffer>& cmd_buffers, Span<VkSemaphore>& wait_sem, Span<VkSemaphore>& signal_sem, VkFence fence=VK_NULL_HANDLE);
		void Submit(const VkPresentInfoKHR& present_info);
		void WaitIdle()const;
		VkQueue Get()const { return handle_; };
		uint32_t GetFamilyIndex()const { return family_index_; }
		uint32_t GetIndex()const { return index_; }
		~VulkanQueue();
	private:
		VulkanDevice::Ptr	device_;
		VkQueue				handle_{ VK_NULL_HANDLE };
		uint32_t			family_index_ = 0;
		uint32_t			index_ = 0;
	};

}

