#include "RHI/VulkanRHI.h"
#include "RHI/VulkanMemAllocator.h"
#include "RHI/VulkanCmdContext.h"
#include "GLM/glm.hpp"

#include <string>
#include <vector>
#include <fstream>

namespace MetaInit
{
	/*for:"this feature is not used, the implementation will 
	  perform its own memory allocations. Since most memory 
	  allocations are off the critical path", so just set null*/
	VkAllocationCallbacks* g_host_alloc = VK_NULL_HANDLE;

	namespace {
		constexpr uint32_t max_phy_devices = 16;
	}

	VulkanDevice::Ptr VulkanDevice::Create(VulkanInstance::Ptr instance, const VkDeviceCreateInfo& device_info)
	{
		
		uint32_t phy_device_count = 0;
		auto ret = vkEnumeratePhysicalDevices(instance->Get(), &phy_device_count, VK_NULL_HANDLE);
		assert(VK_SUCCESS == ret && "not phy device enumereated");
		std::array<VkPhysicalDevice, max_phy_devices> phy_devices;
		vkEnumeratePhysicalDevices(instance->Get(), &phy_device_count, phy_devices.data());
		//find the best phydevice
		auto query_phy_device = [&](VkPhysicalDevice device) {
			uint32_t extension_count;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
			SmallVector<VkExtensionProperties> available_extensions;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());
			for (auto n = 0; n < device_info.enabledExtensionCount; ++n)
			{
				auto ext_iter = std::find(available_extensions.begin(), available_extensions.end(), device_info.ppEnabledExtensionNames[n]);
				if (available_extensions.end() == ext_iter)
				{
					return false;
				}
			}
			return true;
		};
		auto selected_phy_device = std::find_if(phy_devices.begin(), phy_devices.end(), query_phy_device);
		if (selected_phy_device == phy_devices.end())
		{
			throw std::invalid_argument("no suitable device found");
		}
		VulkanDevice::Ptr device_ptr{ new VulkanDevice };
		auto ret = vkCreateDevice(*selected_phy_device, &device_info, g_host_alloc, &device_ptr->handle_);
		assert(ret == VK_SUCCESS && "create device failed");
		device_ptr->device_info_ = device_info;
		vkGetPhysicalDeviceProperties(*selected_phy_device, &device_ptr->device_prop_);

		if (nullptr != device_info.pEnabledFeatures)
		{

		}

		device_ptr->InitialQueues();
		return device_ptr;
	}

	VulkanQueue& VulkanDevice::GetQueue(VulkanDevice::EQueueType queue_type)
	{
		auto family_index = queue_families_[queue_type];

		for(const auto findex:family_index)
		{
			auto& inuse = queue_inuse_[findex];
			std::lock_guard<std::mutex> inuse_lock(mutex_);
			auto iter = std::find(inuse.begin(), inuse.end(), 0);
			if (iter != inuse.end())
			{
				auto index = static_cast<uint32_t>(iter - inuse.begin());
				inuse[index] = 0xffffff; // fixme
				return VulkanQueue(this, findex, index);
			}
		}
		throw std::runtime_error("no supported queue for this type");
	}

	VulkanQueue& VulkanDevice::QueryQueue(QueryCallback query)
	{
		for (uint32_t findex = 0; findex < queue_inuse_.size(); ++findex)
		{
			auto ret = query(phy_devices_, findex);
			if (ret)
			{
				auto& inuse = queue_inuse_[findex];
				std::lock_guard<std::mutex> inuse_lock(mutex_);
				auto iter = std::find(inuse.begin(), inuse.end(), 0);
				if (inuse.end() != iter)
				{
					auto index = static_cast<uint32_t>(iter - inuse.begin());
					inuse[index] = 0xffffff; // fixme
					return VulkanQueue(this, findex, index);
				}

			}
		}
	
		throw std::runtime_error("no supported queue");
	}

	void VulkanDevice::ReleaseQueue(VulkanQueue& queue)
	{
		std::lock_guard<std::mutex> queue_lock(mutex_);
		queue_inuse_[queue.GetFamilyIndex()][queue.GetIndex()] = 0;
	}

	VkSampleCountFlags VulkanDevice::GetMaxUsableSampleCount()const
	{
		//todo
		VkSampleCountFlags counts = std::min(device_prop_.limits.framebufferColorSampleCounts,
												device_prop_.limits.framebufferDepthSampleCounts);
#define CHECK(FLAG) if(counts&FLAG) {return FLAG;}
		CHECK(VK_SAMPLE_COUNT_64_BIT);
		CHECK(VK_SAMPLE_COUNT_32_BIT);
		CHECK(VK_SAMPLE_COUNT_16_BIT);
		CHECK(VK_SAMPLE_COUNT_8_BIT);
		CHECK(VK_SAMPLE_COUNT_4_BIT);
		CHECK(VK_SAMPLE_COUNT_2_BIT);
		return VK_SAMPLE_COUNT_1_BIT;
	}

	uint32_t VulkanDevice::GetMaxColorTargetCount() const
	{
		return device_prop_.limits.maxColorAttachments;
	}

	VkFormatProperties VulkanDevice::GetFormatProperty(VkFormat format) const
	{
		VkFormatProperties format_prop{};
		vkGetPhysicalDeviceFormatProperties(phy_devices_, format, &format_prop);
		return format_prop;
	}

	void VulkanDevice::GetSwapchainSupportInfo() const
	{
	}

	bool VulkanDevice::IsFormatSupported(VkFormat format)const
	{
		VkImageFormatProperties format_prop;
		/*If format is not a supported image format, or if the combination of format, type, tiling, usage, and
			flags is not supported for images, then vkGetPhysicalDeviceImageFormatProperties returns VK_ERROR_FORMAT_NOT_SUPPORTED.*/
		auto ret = vkGetPhysicalDeviceImageFormatProperties(phy_devices_, format, VK_IMAGE_TYPE_2D,
															VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, 0, &format_prop);
		return ret != VK_ERROR_FORMAT_NOT_SUPPORTED;
	}
	
	void VulkanDevice::WaitIdle()const
	{
		vkDeviceWaitIdle(handle_);
	}

	VulkanDevice::~VulkanDevice()
	{
		if (VK_NULL_HANDLE != handle_)
		{
			vkDestroyDevice(handle_, g_host_alloc);
		}
	}

	void VulkanDevice::InitialQueues()
	{
		uint32_t family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(phy_devices_, &family_count, nullptr);
		SmallVector<VkQueueFamilyProperties> family_props(family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(phy_devices_, &family_count, family_props.data());

		queue_inuse_.resize(family_count);
		for (auto n = 0; n < family_count; ++n)
		{
			auto& inuse = queue_inuse_[n];
			inuse.resize(family_props[n].queueCount);
			std::fill(inuse.begin(), inuse.end(), 0);
		}

		auto queue_creator = [&](EQueueType queue_type) {
			VkQueueFlags queue_flags = 0;
			switch (queue_type)
			{
			case GRAPHICS:
				queue_flags = VK_QUEUE_GRAPHICS_BIT;
				break;
			case COMPUTE:
				queue_flags = VK_QUEUE_COMPUTE_BIT;
				break;
			case TRANSFER:
				queue_flags = VK_QUEUE_TRANSFER_BIT;
				break;
			case ALLIN:
				queue_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
				break;
			default:
				throw std::invalid_argument("not supported queue type");
			}

			for (auto findex = 0; findex < family_count; ++findex)
			{
				auto& prop = family_props[findex];
				if (queue_flags & prop.queueFlags)
				{
					queue_families_[queue_type].push_back(findex);
				}
			}
		};

		queue_creator(VulkanDevice::EQueueType::GRAPHICS);
		queue_creator(VulkanDevice::EQueueType::COMPUTE);
		queue_creator(VulkanDevice::EQueueType::TRANSFER);
		queue_creator(VulkanDevice::EQueueType::ALLIN);
	}

	VulkanInstance::Ptr VulkanInstance::Create(const VkInstanceCreateInfo& params)
	{
		VulkanInstance::Ptr instance_ptr(new VulkanInstance);

		/*check instance extensions*/
		if (params.enabledExtensionCount>0)
		{
			
			uint32_t extensions_count = 0;
			vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &extensions_count, VK_NULL_HANDLE);

			if (extensions_count <= 0)
			{
				throw std::runtime_error("error while get instance extensions count");
			}

			SmallVector<VkExtensionProperties> extensions(extensions_count);
			vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &extensions_count, extensions.data());

			auto extension_check = [&](const char* ext_name) {
				auto iter = std::find_if(extensions.begin(), extensions.end(), [&](VkExtensionProperties prop) {return strcmp(prop.extensionName, ext_name)==0;});
				if (iter == extensions.end())
					return true;
				return false;
			};

			if (std::any_of(params.ppEnabledExtensionNames, params.ppEnabledExtensionNames+
								params.enabledExtensionCount, extension_check))
			{
				throw std::invalid_argument("some extension not support by instance");
			}
		}

		//check instance layer support
		if (params.enabledLayerCount > 0)
		{
			uint32_t layers_count = 0;
			vkEnumerateInstanceLayerProperties(&layers_count, nullptr);
			if (layers_count > 0)
			{
				SmallVector<VkLayerProperties> layers(layers_count);
				vkEnumerateInstanceLayerProperties(&layers_count, layers.data());
				auto layer_check = [&](const char* layer_name) {
					auto iter = std::find_if(layers.begin(), layers.end(), [&](VkLayerProperties prop) {return strcmp(layer_name, prop.layerName) == 0;});
					if (iter == layers.end())
						return true;
					return false;
				};

				if (std::any_of(params.ppEnabledLayerNames, params.ppEnabledLayerNames + params.enabledLayerCount, layer_check))
				{
					throw std::invalid_argument("some layers not support by instance");
				}
			}
		}

		//create instance
		auto ret = vkCreateInstance(&params, g_host_alloc, &instance_ptr->handle_);
		if (ret != VK_SUCCESS)
		{
			throw std::runtime_error("create vulkan instance failed");
		}

		return instance_ptr;
	}

	VulkanQueue::VulkanQueue(VulkanDevice* device, uint32_t family_index, uint32_t index):device_(device), handle_(handle)
	{
		//todo other work
		vkGetDeviceQueue(device->Get(), family_index, index, &handle_);
	}

	void VulkanQueue::Submit(Span<VulkanCmdBuffer>& cmd_buffers, Span<VkSemaphore>& wait_sem, Span<VkSemaphore>& signal_sem)
	{
		VkSubmitInfo submit_info{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		memset(&submit_info, 0, sizeof(VkSubmitInfo));
		submit_info.commandBufferCount = cmd_buffers.size();
		SmallVector<VkCommandBuffer> cmd_handles;
		for (auto n = 0; n < cmd_buffers.size(); ++n)
		{
			cmd_handles[n] = cmd_buffers[n].Get();
		}
		submit_info.pCommandBuffers = cmd_handles.data(); //fixme

		if (!wait_sem.empty())
		{
			submit_info.waitSemaphoreCount = wait_sem.size();
			submit_info.pWaitSemaphores = wait_sem.data();
		}

		if (!signal_sem.empty())
		{
			submit_info.signalSemaphoreCount = signal_sem.size();
			submit_info.pSignalSemaphores = signal_sem.data();
		}

		vkQueueSubmit(handle_, 1, &submit_info, VK_NULL_HANDLE); //FIXME
	}

	void VulkanQueue::Submit(const VkPresentInfoKHR& present_info)
	{
		vkQueuePresentKHR(handle_, &present_info);
	}

	void VulkanQueue::WaitIdle()const
	{
		vkQueueWaitIdle(handle_);
	}

	VulkanQueue::~VulkanQueue()
	{
		//noop, detroy queue while destroying device
		if (nullptr != device_)
		{
			device_->ReleaseQueue(*this);
		}

	}


}