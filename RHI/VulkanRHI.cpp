#include "RHI/VulkanRHI.h"
#include "RHI/VulkanMemAllocator.h"
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

	VulkanDevice::Ptr VulkanDevice::Create(const VkDeviceCreateInfo& device_info)
	{
		VulkanDevice::Ptr device_ptr{ new VulkanDevice};
		auto ret = vkCreateDevice(nullptr, &device_info, g_host_alloc, &device_ptr->device_);
		assert(ret == VK_SUCCESS && "create device failed");
		device_ptr->device_info_ = device_info;

		if (nullptr != device_info.pEnabledFeatures)
		{

		}
		return device_ptr;
	}

	VkQueue VulkanDevice::GetQueue(uint32_t family_index, uint32_t index)const
	{
		VkQueue queue{ VK_NULL_HANDLE };
		vkGetDeviceQueue(handle_, family_index, index, &queue);
		return queue;
	}

	VkQueue VulkanDevice::GetQueue(VkQueueFlags flags)const
	{
		uint32_t family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(phy_devices_, &family_count, nullptr);
		SmallVector<VkQueueFamilyProperties> family_props(family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(phy_devices_, &family_count, family_props.data());

		for (auto findex = 0; findex < family_props.size(); ++findex)
		{
			auto& prop = family_props[findex];
			if (prop.queueFlags & flags)
			{
				//to check flags
				return GetQueue(findex, 0);
			}
		}

		return VK_NULL_HANDLE;
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
	
	/*
	VkPhysicalDevice VulkanDevice::operator[](uint32_t index)
	{
		assert(index < phy_devices_.size());
		return phy_devices_[index];
	}
	*/
	void VulkanDevice::WaitIdle()const
	{
		vkDeviceWaitIdle(handle_);
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


}