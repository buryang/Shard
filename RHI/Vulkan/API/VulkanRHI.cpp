#include "eastl/algorithm.h"
#include "Utils/CommonUtils.h"
#include "RHI/Vulkan/API/VulkanMemAllocator.h"
#include "RHI/Vulkan/API/VulkanCmdContext.h"
#include "RHI/Vulkan/API/VulkanHelperStruct.h"
#include "RHI/Vulkan/API/VulkanRHI.h"

#define ADD_EXT_IF(CONDITION, EXT_NAME) if (CONDITION) { extensions.emplace_back(EXT_NAME); }
#define ADD_LAYER_IF(CONDITION, LAYER_NAME) if(CONDITION) { layers.emplace_back(LAYER_NAME); }

namespace MetaInit
{
	/*for:"this feature is not used, the implementation will 
	  perform its own memory allocations. Since most memory 
	  allocations are off the critical path", so just set null*/
	VkAllocationCallbacks* g_host_alloc = VK_NULL_HANDLE;

	//whether use ASYNC COMPUTE
	REGIST_PARAM_TYPE(BOOL, DEVICE_ASYNC_COMPUTE, true);

	static inline void MakeBindlessDeviceDescriptorIndexingFeatures(VkPhysicalDeviceDescriptorIndexingFeatures& indexing_features) {
		memset(&indexing_features, 1, sizeof(indexing_features)); //default enable everything
		indexing_features.pNext = nullptr;
		indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		//https://blog.traverseresearch.nl/bindless-rendering-setup-afeb678d77fc
		indexing_features.descriptorBindingUniformBufferUpdateAfterBind = false;//
		indexing_features.shaderInputAttachmentArrayDynamicIndexing = false;//
		indexing_features.shaderInputAttachmentArrayNonUniformIndexing = false;//
	}

	static inline VkDeviceCreateInfo MakeDeviceCreateInfo(VkDeviceCreateFlags flags, const Span<VkDeviceQueueCreateInfo>& queue_infos, const Span<const char*>& ext_infos)
	{
		VkDeviceCreateInfo device_info;
		memset(&device_info, 0, sizeof(device_info));
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.flags = flags;
		device_info.queueCreateInfoCount = queue_infos.size();
		device_info.pQueueCreateInfos = queue_infos.data();
		device_info.enabledExtensionCount = ext_infos.size();
		device_info.ppEnabledExtensionNames = ext_infos.data();
		return device_info;
	}

	static inline VkInstanceCreateInfo MakeInstanceCreateInfo(VkInstanceCreateFlags flags, const VkApplicationInfo& app_info, const Span<const char*> ext_infos, const Span<const char*>& layer_infos)
	{
		VkInstanceCreateInfo instance_info;
		memset(&instance_info, 0, sizeof(instance_info));
		instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instance_info.flags = flags;
		instance_info.pApplicationInfo = &app_info;
		instance_info.enabledLayerCount = layer_infos.size();
		instance_info.ppEnabledLayerNames = layer_infos.data();
		instance_info.enabledExtensionCount = ext_infos.size();
		instance_info.ppEnabledExtensionNames = ext_infos.data();
		return instance_info;
	}

	VulkanDevice::SharedPtr VulkanDevice::Create(VulkanInstance::SharedPtr instance)
	{
		VulkanDevice::SharedPtr device_ptr{ new VulkanDevice };
		SmallVector<const char*> extensions;
		ADD_EXT_IF(GET_PARAM_TYPE_VAL(BOOL, DEVICE_MEMORY_BUDGET), VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
		ADD_EXT_IF(GET_PARAM_TYPE_VAL(BOOL, DEVICE_MEMORY_REQUIRE), VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
		ADD_EXT_IF(GET_PARAM_TYPE_VAL(BOOL, DEVICE_DEDICATED_ALLOC), VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);

		auto selected_phy_device = SelectSuitAbleDevice(instance, extensions);

		uint32_t queue_property_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(selected_phy_device, &queue_property_count, nullptr);
		SmallVector<VkQueueFamilyProperties> queue_properties(queue_property_count);
		vkGetPhysicalDeviceQueueFamilyProperties(selected_phy_device, &queue_property_count, queue_properties.data());

		SmallVector<VkDeviceQueueCreateInfo, 3> queue_infos;
		{
			const auto find_queue_family = [&](VkQueueFlags flags) {
				for (auto n = 0; n < queue_properties.size(); ++n) {
					const auto& prop = queue_properties[n];
					if ((prop.queueFlags & ~VK_QUEUE_SPARSE_BINDING_BIT) == flags) {
						return n;
					}
				}
				PLOG(ERROR) << "do not find suitable queue";
			};

			auto gfx_family = find_queue_family(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT);
			VkDeviceQueueCreateInfo gfx_queue;
			memset(&gfx_queue, 0, sizeof(gfx_queue));
			gfx_queue.queueFamilyIndex = gfx_family;
			gfx_queue.queueCount = 1;
			queue_infos.emplace_back(gfx_queue);
			VulkanQueue::RegistFamily(VulkanQueue::EQueueType::eAllIn, gfx_family);

			/*
				auto transfer_family = find_queue_family(VK_QUEUE_TRANSFER_BIT);
				VkDeviceQueueCreateInfo transfer_queue;
				memset(&gfx_queue, 0, sizeof(transfer_queue));
				transfer_queue.queueFamilyIndex = transfer_family;
				transfer_queue.queueCount = 1;
				queue_infos.emplace_back(transfer_queue);
				VulkanQueue::RegistFamily(VulkanQueue::EQueueType::eTransfer, transfer_family);
			*/
			
			if (GET_PARAM_TYPE_VAL(BOOL, DEVICE_ASYNC_COMPUTE)) {
				auto compute_family = find_queue_family(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT);
				VkDeviceQueueCreateInfo compute_queue;
				memset(&compute_queue, 0, sizeof(compute_queue));
				compute_queue.queueFamilyIndex = compute_family;
				compute_queue.queueCount = 1;
				queue_infos.emplace_back(compute_queue);
				VulkanQueue::RegistFamily(VulkanQueue::EQueueType::eNonGFX, compute_family);
			}
		}

		auto device_info = MakeDeviceCreateInfo(0x0, queue_infos, extensions);
		//enable descriptor indexing feature
		StructureChain<VkPhysicalDeviceDescriptorIndexingFeatures> device_extentions;
		if (GET_PARAM_TYPE_VAL(BOOL, RHI_RESOURCE_BINDLESS)) {
			auto indexing_features = device_extentions.get<VkPhysicalDeviceDescriptorIndexingFeatures>();
			MakeBindlessDeviceDescriptorIndexingFeatures(indexing_features);
			device_info.pNext = &indexing_features;
		}
		auto ret = vkCreateDevice(selected_phy_device, &device_info, g_host_alloc, &device_ptr->handle_);
		PCHECK(VK_SUCCESS == ret ) << "vulkan create logic device failed";

		//initial device related members
		{
			device_ptr->back_end_ = selected_phy_device;
			VkPhysicalDeviceProperties2 back_end_properties2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
			device_ptr->back_end_descrptor_indexing_properties_ = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES, .pNext=VK_NULL_HANDLE };
			back_end_properties2.pNext = &device_ptr->back_end_descrptor_indexing_properties_;
			vkGetPhysicalDeviceProperties2(selected_phy_device, &back_end_properties2);
			device_ptr->back_end_properties_ = back_end_properties2.properties;
		}
		return device_ptr;
	}

	VkSampleCountFlags VulkanDevice::GetMaxUsableSampleCount() const
	{
		//todo
		VkSampleCountFlags counts = std::min(back_end_properties_.limits.framebufferColorSampleCounts,
										back_end_properties_.limits.framebufferDepthSampleCounts);
#define CHECK_BIT(FLAG) if(counts & FLAG) { return FLAG; }
		CHECK_BIT(VK_SAMPLE_COUNT_64_BIT);
		CHECK_BIT(VK_SAMPLE_COUNT_32_BIT);
		CHECK_BIT(VK_SAMPLE_COUNT_16_BIT);
		CHECK_BIT(VK_SAMPLE_COUNT_8_BIT);
		CHECK_BIT(VK_SAMPLE_COUNT_4_BIT);
		CHECK_BIT(VK_SAMPLE_COUNT_2_BIT);
		return VK_SAMPLE_COUNT_1_BIT;
#undef CHECK_BIT
	}

	VkFormatProperties VulkanDevice::GetFormatProperty(VkFormat format) const
	{
		VkFormatProperties format_prop{};
		vkGetPhysicalDeviceFormatProperties(back_end_, format, &format_prop);
		return format_prop;
	}

	void VulkanDevice::GetSwapchainSupportInfo() const
	{
		return;
	}

	bool VulkanDevice::IsFormatSupported(VkFormat format)const
	{
		VkImageFormatProperties format_prop{};
		/*If format is not a supported image format, or if the combination of format, type, tiling, usage, and
			flags is not supported for images, then vkGetPhysicalDeviceImageFormatProperties returns VK_ERROR_FORMAT_NOT_SUPPORTED.*/
		auto ret = vkGetPhysicalDeviceImageFormatProperties(back_end_, format, VK_IMAGE_TYPE_2D,
															VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, 0, &format_prop);
		return ret != VK_ERROR_FORMAT_NOT_SUPPORTED;
	}
	
	void VulkanDevice::WaitIdle() const
	{
		vkDeviceWaitIdle(handle_);
	}

	VulkanDevice::~VulkanDevice()
	{
		if (VK_NULL_HANDLE != handle_)
		{
			WaitIdle();
			vkDestroyDevice(handle_, g_host_alloc);
		}
	}

	VkPhysicalDevice VulkanDevice::SelectSuitAbleDevice(VulkanInstance::SharedPtr instance, const Span<const char*> extensions)
	{
		uint32_t phy_device_count = 0;
		auto ret = vkEnumeratePhysicalDevices(instance->Get(), &phy_device_count, VK_NULL_HANDLE);
		assert(VK_SUCCESS == ret && "not phy device enumereated");
		SmallVector<VkPhysicalDevice> phy_devices(phy_device_count);
		vkEnumeratePhysicalDevices(instance->Get(), &phy_device_count, phy_devices.data());
		//find the best phydevice
		const auto query_phy_device = [&](auto device)->bool {
			uint32_t extension_count;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
			SmallVector<VkExtensionProperties> available_extensions;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());
			for (auto n = 0; n < extensions.size(); ++n)
			{
				auto ext_iter = std::find(available_extensions.begin(), available_extensions.end(), extensions[n]);
				if (available_extensions.end() == ext_iter)
				{
					return false;
				}
			}
			return true;
		};
		auto selected_phy_device = eastl::find_if(phy_devices.begin(), phy_devices.end(), query_phy_device);
		CHECK(selected_phy_device != phy_devices.end()) << "vulkan can not find physic device suitable";
		return *selected_phy_device;
	}

	VulkanInstance::SharedPtr VulkanInstance::Create()
	{
		VkApplicationInfo app_info;
		memset(&app_info, 0, sizeof(app_info));
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pEngineName = "MetaInit";
		app_info.engineVersion = 1;
		app_info.apiVersion = 0;

		SmallVector<const char*> extensions;
		ADD_EXT_IF(GET_PARAM_TYPE_VAL(BOOL, INSTANCE_DEBUG_REPORT_EXT), "VK_EXT_debug_report");
		ADD_EXT_IF(GET_PARAM_TYPE_VAL(BOOL, INSTANCE_DEBUG_UTILS_EXT), "VK_EXT_debug_utils");
		SmallVector<const char*> layers;

		VulkanInstance::SharedPtr instance_ptr(new VulkanInstance);
		const auto& instance_info = MakeInstanceCreateInfo(0x0, app_info, extensions, layers);

		/*check instance extensions*/
		if (instance_info.enabledExtensionCount > 0)
		{
			uint32_t extensions_count = 0;
			vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &extensions_count, VK_NULL_HANDLE);
			SmallVector<VkExtensionProperties> extensions(extensions_count);
			vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &extensions_count, extensions.data());

			const auto extension_check = [&](const char* ext_name) {
				auto iter = std::find_if(extensions.begin(), extensions.end(), [&](VkExtensionProperties prop) {return strcmp(prop.extensionName, ext_name)==0;});
				if (iter == extensions.end())
					return true;
				return false;
			};

			if (eastl::any_of(instance_info.ppEnabledExtensionNames,
				instance_info.ppEnabledExtensionNames + instance_info.enabledExtensionCount, extension_check))
			{
				PLOG(ERROR) << "not all extension supported";
			}
		}

		//check instance layer support
		if (instance_info.enabledLayerCount > 0)
		{
			uint32_t layers_count = 0;
			vkEnumerateInstanceLayerProperties(&layers_count, nullptr);
			if (layers_count > 0)
			{
				SmallVector<VkLayerProperties> layers(layers_count);
				vkEnumerateInstanceLayerProperties(&layers_count, layers.data());
				const auto layer_check = [&](const char* layer_name) {
					auto iter = std::find_if(layers.begin(), layers.end(), [&](VkLayerProperties prop) {return strcmp(layer_name, prop.layerName) == 0;});
					if (iter == layers.end())
						return true;
					return false;
				};

				if (std::any_of(instance_info.ppEnabledLayerNames, instance_info.ppEnabledLayerNames + instance_info.enabledLayerCount, layer_check))
				{
					PLOG(ERROR) << " : some layers not support by instance";
				}
			}
		}
		//create instance
		auto ret = vkCreateInstance(&instance_info, g_host_alloc, &instance_ptr->handle_);
		PCHECK(ret == VK_SUCCESS) << "create vulkan instance failed";
		return instance_ptr;
	}

	VulkanQueue::VulkanQueue(uint32_t family_index, uint32_t index):family_index_(family_index), index_(index)
	{
		vkGetDeviceQueue(GetGlobalDevice(), family_index, index, &handle_);
	}

	VulkanQueue::SharedPtr VulkanQueue::Create(VulkanQueue::EQueueType queue_type)
	{
		VulkanQueue::SharedPtr queue;
		auto family_index = queue_families_[VulkanQueue::EQueueType::eAllIn];
		if (GET_PARAM_TYPE_VAL(BOOL, DEVICE_ASYNC_COMPUTE) && (VulkanQueue::EQueueType::eCompute == queue_type ||
			VulkanQueue::EQueueType::eNonGFX == queue_type))
		{
			family_index = queue_families_[VulkanQueue::EQueueType::eNonGFX];
		}
		queue.reset(new VulkanQueue(family_index, 0));
		return queue;
	}

	void VulkanQueue::Submit(VulkanCmdBuffer::SharedPtr cmd_buffer, const Span<VkSemaphore>& wait_semaphores, const Span<VkSemaphore>& signal_semaphores, VkFence fence)
	{
		VkSubmitInfo submit_info{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		memset(&submit_info, 0, sizeof(VkSubmitInfo));
		submit_info.commandBufferCount = 1;
		const VkCommandBuffer temp_cmd_buffers[] = { cmd_buffer->Get() };
		submit_info.pCommandBuffers = temp_cmd_buffers;

		if (!wait_semaphores.empty())
		{
			submit_info.waitSemaphoreCount = wait_semaphores.size();
			submit_info.pWaitSemaphores = wait_semaphores.data();
		}
		if (!signal_semaphores.empty())
		{
			submit_info.signalSemaphoreCount = signal_semaphores.size();
			submit_info.pSignalSemaphores = signal_semaphores.data();
		}
		vkQueueSubmit(handle_, 1, &submit_info, fence); 
	}

	void VulkanQueue::Submit(const VkPresentInfoKHR& present_info)
	{
		vkQueuePresentKHR(handle_, &present_info);
	}

	void VulkanQueue::WaitIdle()const
	{
		vkQueueWaitIdle(handle_);
	}
}