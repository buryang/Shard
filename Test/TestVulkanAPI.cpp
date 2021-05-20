//#include "RHI/VulkanRHI.h"

#include "gtest/gtest.h"
#include <vector>
#include <optional>

//using namespace MetaInit;

TEST(TEST_RHI_MODULE, TEST_VULKAN_INIT)
{
	EXPECT_TRUE(true);
}

TEST(TEST_RHI_MODULE, TEST_VULKAN_PIPLINE)
{
	EXPECT_TRUE(true);
}

#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"

TEST(TEST_RHI_MODULE, TEST_API_TUTORIAL)
{
	uint32_t extension_count = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
	EXPECT_GE(extension_count, 1) << "NV GPU should support more than 1 extensions";
	std::vector<VkExtensionProperties> extension_props(extension_count);
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extension_props.data());
	for (const auto& prop : extension_props)
	{
		std::cout << prop.extensionName << ":" << prop.specVersion << "\n";
	}
	std::cout << std::endl;
	VkInstanceCreateInfo instance_info;
	instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_info.pNext = nullptr;
	instance_info.pApplicationInfo = nullptr;
	instance_info.enabledLayerCount = 0;
	VkInstance instance;
	VkResult result = vkCreateInstance(&instance_info, nullptr, &instance);
	ASSERT_EQ(result, VK_SUCCESS);
	EXPECT_TRUE(instance != nullptr);

	//query device
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	EXPECT_LE(device_count, 1) << "should have at least one device";
	std::vector<VkPhysicalDevice> phy_devices(device_count);
	vkEnumeratePhysicalDevices(instance, &device_count, phy_devices.data());
	for (const auto& phd : phy_devices)
	{
		VkPhysicalDeviceProperties device_props;
		vkGetPhysicalDeviceProperties(phd, &device_props);
		std::cout << std::uppercase << device_props.deviceName << std::endl;
		std::cout << device_props.deviceType << std::endl;
		std::cout << "max vertexes: " << device_props.limits.maxVertexInputBindings << std::endl;

		uint32_t device_extension_count = 0;
		vkEnumerateDeviceExtensionProperties(phd, nullptr, &device_extension_count, nullptr);
		ASSERT_GT(device_extension_count, 0) << "device extension for NVIDIA";
		std::vector<VkExtensionProperties> device_extensions(device_extension_count);
		vkEnumerateDeviceExtensionProperties(phd, nullptr, &device_extension_count, device_extensions.data());
		std::cout << "------------------------------------" << std::endl;
		for (const auto& ext : device_extensions)
		{
			std::cout << ext.extensionName << ":" << ext.specVersion << "\n";
		}

		uint32_t device_layer_count = 0;
		vkEnumerateDeviceLayerProperties(phd, &device_layer_count, nullptr);
		std::cout << "+++++++++++++++++++++++++++++++++++++" << std::endl;
		std::vector<VkLayerProperties> device_layers;
		vkEnumerateDeviceLayerProperties(phd, &device_layer_count, device_layers.data());
		for (const auto& layer : device_layers)
		{
			std::cout << layer.layerName << " : " << layer.specVersion << "\n";
		}
	}
	std::cout << std::endl;
	//create logic device
	VkPhysicalDeviceFeatures device_feature{};
	vkGetPhysicalDeviceFeatures(phy_devices[0], &device_feature);
	ASSERT_TRUE(device_feature.geometryShader);
	VkDeviceCreateInfo device_info{};
	device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_info.enabledExtensionCount = 0; //FIXME
	device_info.enabledLayerCount = 0;
	device_info.pEnabledFeatures = &device_feature;

	//query device queue
	std::optional<uint32_t> family_indice;
	auto queue_family = [&](const uint32_t queue_flags)
	{
		uint32_t family_count = 0;
		int queue_count = -1;
		vkGetPhysicalDeviceQueueFamilyProperties(phy_devices[0], &family_count, nullptr);
		std::vector<VkQueueFamilyProperties> familys_props(family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(phy_devices[0], &family_count, familys_props.data());
		
		uint32_t index = 0;
		/*https://stackoverflow.com/questions/52015730/vulkan-queue-families?answertab=votes#tab-top
		  Queue flags: {Graphics | Compute | Transfer | SparseBinding}
		*/
		for (const auto& family : familys_props)
		{
			std::cout << family.queueFlags << std::endl;
			if (family.queueFlags & queue_flags)
			{
				family_indice = index;
				queue_count = family.queueCount;
				break;
			}
			++index;
		}

		return queue_count;
	};
	
	auto queue_count = queue_family(VK_QUEUE_COMPUTE_BIT);
	ASSERT_TRUE(family_indice.has_value());
	VkDeviceQueueCreateInfo queue_info{};
	queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info.queueFamilyIndex = family_indice.value();
	queue_info.queueCount = queue_count; //1;
	constexpr float queue_prior = 1.0f;
	queue_info.pQueuePriorities = &queue_prior;
	queue_info.pNext = nullptr;

	device_info.pQueueCreateInfos = &queue_info;
	device_info.queueCreateInfoCount = 1;
	VkDevice device{};
	result = vkCreateDevice(phy_devices[0], &device_info, nullptr, &device);
	ASSERT_TRUE(result == VK_SUCCESS) << __LINE__ << "create device failed";
	VkQueue graph_queue;
	vkGetDeviceQueue(device, family_indice.value(), 0, &graph_queue);
	ASSERT_TRUE(graph_queue != VK_NULL_HANDLE);

	//gtest other things
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);
}

/*render a simple triangle for test*/
TEST(TEST_RHI_MODULE, TEST_SIMPLE_TRIANGLE)
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	auto window = glfwCreateWindow(800, 600, "TEST_TRIANGLE", VK_NULL_HANDLE, VK_NULL_HANDLE);

	ASSERT_TRUE(glfwVulkanSupported()) << "glfw do not support vulkan";

}