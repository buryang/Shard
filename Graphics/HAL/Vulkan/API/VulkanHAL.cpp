#include "Utils/CommonUtils.h"
#include "HAL/Vulkan/API/VulkanCmdContext.h"
#include "HAL/Vulkan/API/VulkanStat.h"
#include "HAL/Vulkan/API/VulkanHAL.h"
#include "eastl/algorithm.h"

#define ADD_EXT_IF(CONDITION, EXT_NAME) if (CONDITION) { extensions.emplace_back(EXT_NAME); }
#define ADD_LAYER_IF(CONDITION, LAYER_NAME) if(CONDITION) { layers.emplace_back(LAYER_NAME); }

namespace Shard
{
    /*for:"this feature is not used, the implementation will 
      perform its own memory allocations. Since most memory 
      allocations are off the critical path", so just set null*/
#ifdef VULKAN_USER_ALLOCATION
    namespace Memory
    {
        extern Utils::ScalablePoolAllocator<uint8_t>* g_hal_allocator;
        /*Memory alignment maybe an issue to implement VkAllocationCallback (there is
          no _aligned_realoc function available, but only _aligned_malloc and _aligned_free).
          But only if Vulkan requests alignments bigger than malloc's default 
          (8 bytes for x86, 16 for AMD64, etc. must check ARM defaults). But so far, seens 
          Vulkan actually request memory with lower alignment than malloc() defaults,*/
        static void* pfn_vulkan_alloc(void* user_data, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope) { 
            const auto alloc_size = sizeof(uintptr_t) + size; //save size in extra memory
            uintptr_t* data = reinterpret_cast<uintptr_t*>(g_hal_allocator->allocate(alloc_size));
            *data++ = alloc_size;
            return data;
        }
        static void* pfn_vulkan_realloc(void*, void* orignal, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope) {
            void* new_data = pfn_vulkan_alloc(nullptr, size, alignment, allocation_scope); //todo
            memcpy(new_data, orignal, size);
            pfn_vulkan_free(nullptr, orignal);
            return new_data;
        }
        static void pfn_vulkan_free(void*, void* memory) {
            uintptr_t* data = reinterpret_cast<uintptr_t*>(memory) - 1;
            g_hal_allocator->deallocate(reinterpret_cast<uint8_t*>(data), *data);
        }
        VkAllocationCallbacks g_vulkan_allocator{ .pUserData = nullptr,
                                                    .pfnAllocation = &pfn_vulkan_alloc,
                                                    .pfnReallocation = &pfn_vulkan_realloc,
                                                    .pfnFree = &pfn_vulkan_free,
                                                    .pfnInternalAllocation = nullptr,
                                                    .pfnInternalFree = nullptr };
    }
    VkAllocationCallbacks* g_host_alloc_vulkan = &g_vulkan_allocator;
#else
    VkAllocationCallbacks* g_host_alloc_vulkan = nullptr;
#endif

    static inline void MakeBindlessDeviceDescriptorIndexingFeatures(VkPhysicalDeviceDescriptorIndexingFeatures& indexing_features) {
        memset(&indexing_features, 1, sizeof(indexing_features)); //default enable everything
        indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        //https://blog.traverseresearch.nl/bindless-rendering-setup-afeb678d77fc
        indexing_features.descriptorBindingUniformBufferUpdateAfterBind = false;//
        indexing_features.shaderInputAttachmentArrayDynamicIndexing = false;//
        indexing_features.shaderInputAttachmentArrayNonUniformIndexing = false;//
    }

    static inline void MakeFragmentVRSFeature(VkPhysicalDeviceFragmentShadingRateFeaturesKHR& vrs_features) {
        memset(&vrs_features, 0, sizeof(vrs_features));
        vrs_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
        vrs_features.pipelineFragmentShadingRate = VK_TRUE;
    }

    static inline void MakeDeviceCreateInfo(VkDeviceCreateInfo& device_info, VkDeviceCreateFlags flags, const Span<VkDeviceQueueCreateInfo>& queue_infos, const Span<const char*>& ext_infos)
    {
        memset(&device_info, 0u, sizeof(device_info));
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.flags = flags;
        device_info.queueCreateInfoCount = queue_infos.empty() ? 0u : queue_infos.size();
        device_info.pQueueCreateInfos = queue_infos.empty() ? nullptr : queue_infos.data();
        device_info.enabledExtensionCount = ext_infos.empty() ? 0u : ext_infos.size();
        device_info.ppEnabledExtensionNames = ext_infos.empty() ? nullptr : ext_infos.data();
    }

    static inline void MakeInstanceCreateInfo(VkInstanceCreateInfo& instance_info, VkInstanceCreateFlags flags, const VkApplicationInfo& app_info, const Span<const char*> ext_infos, const Span<const char*>& layer_infos)
    {
        memset(&instance_info, 0u, sizeof(instance_info));
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.flags = flags;
        instance_info.pApplicationInfo = &app_info;
        instance_info.enabledLayerCount = layer_infos.empty() ? 0u : layer_infos.size();
        instance_info.ppEnabledLayerNames = layer_infos.empty() ? nullptr : layer_infos.data();
        instance_info.enabledExtensionCount = ext_infos.empty() ? 0u : ext_infos.size();
        instance_info.ppEnabledExtensionNames = ext_infos.empty() ? nullptr : ext_infos.data();
    }

    static inline void MakeQueueCreateInfo(VkDeviceQueueCreateInfo& queue_info, VkDeviceQueueCreateFlags flags, uint32_t family_index, uint32_t queue_count, const float* priorities)
    {
        memset(&queue_info, 0u, sizeof(queue_info));
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.flags = flags;
        queue_info.queueFamilyIndex = family_index;
        queue_info.queueCount = queue_count;
        queue_info.pQueuePriorities = priorities;
    }

#ifdef WIN32
    static inline void MakeSurfaceCreateInfo(VkWin32SurfaceCreateInfoKHR& create_info, HWND wind) {
        create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        create_info.pNext = nullptr;
        create_info.hwnd = wind;
        create_info.hinstance = GetModuleHandle(nullptr);
    }
#endif

#if defined(DEVELOP_DEBUG_TOOLS) && defined(DEBUG_VULKAN)
    static inline void MakeDebugUtilsMessengerCreateInfoEXT(VkDebugUtilsMessengerCreateInfoEXT& messenger_info, void* user_data) {
        memset(&messenger_info, 0, sizeof(messenger_info));
        messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        messenger_info.pfnUserCallback = DebugUtilsMessengerCallbackEXT;
        messenger_info.pUserData = user_data;
    }

    static inline void MakeDebugReportCallbackCreateInfoEXT(VkDebugReportCallbackCreateInfoEXT& report_info, void* user_data) {
        memset(&report_info, 0, sizeof(report_info));
        report_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        report_info.flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
        report_info.pfnCallback = DebugReportCallbackEXT;
        report_info.pUserData = user_data;

    }
#endif 

#if defined(DEVELOP_DEBUG_TOOLS) || defined(DEBUG_VULKAN)
    static inline MakeDebugMessagerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& messenger_info)
    {
        static constexpr VkDebugUtilsMessageSeverityFlagsEXT kseverities =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

        static constexpr VkDebugUtilsMessageTypeFlagsEXT kmessages =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        
        memset(&messenger_info, 0u, sizeof(messenger_info));
        messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messenger_info.messageSeverity = kseverities;
        messenger_info.messageType = kmessages;
        messenger_info.pfnUserCallback = &DebugUtilsMessengerCallbackEXT;
    }

    static inline void MakeDebugReportCreateInfo(VkDebugReportCallbackCreateInfoEXT& reporter_info)
    {
        memset(&reporter_info, 0u, sizeof(reporter_info));
        reporter_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        reporter_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        reporter_info.pfnCallback = &DebugReportCallbackEXT;
    }
#endif
    //order possible flags
    static inline uint32_t GetQueueTypeIndex(VkQueueFlags flags) {
        switch (flags)
        {
        case VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT:
        default:
            return 0u;
        case VK_QUEUE_GRAPHICS_BIT:
            return 1u;
        case VK_QUEUE_COMPUTE_BIT:
            return 2u;
        case VK_QUEUE_TRANSFER_BIT:
            return 3u;
        }
    }

    VulkanInstance::SharedPtr VulkanInstance::Create(const Span<const char*>& require_extensions)
    {
        VkApplicationInfo app_info;
        memset(&app_info, 0, sizeof(app_info));
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pEngineName = "Shard";
        app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        app_info.apiVersion = VK_API_VERSION_1_2;

        SmallVector<const char*> extensions;
        eastl::move(require_extensions.begin(), require_extensions.end(), eastl::back_inserter(extensions));

        ADD_EXT_IF(GET_PARAM_TYPE_VAL(BOOL, VULKAN_INSTANCE_DEBUG_REPORT_EXT), "VK_EXT_debug_report");
        ADD_EXT_IF(GET_PARAM_TYPE_VAL(BOOL, VULKAN_INSTANCE_DEBUG_UTILS_EXT), "VK_EXT_debug_utils");
        ADD_EXT_IF(GET_PARAM_TYPE_VAL(BOOL, VULKAN_INSTANCE_MEMORY_BUDGET_EXT), "VK_EXT_memory_budget");
        SmallVector<const char*> layers;
#if defined(DEVELOP_DEBUG_TOOLS) && defined(DEBUG_VULKAN)
        layers.emplace_back("VK_LAYER_KHRONOS_validation");
#endif

        VulkanInstance::SharedPtr instance_ptr(new VulkanInstance);
        VkInstanceCreateInfo instance_info{};
        MakeInstanceCreateInfo(instance_info, 0x0, app_info, extensions, layers);
        
#if defined(DEVELOP_DEBUG_TOOLS) && defined(DEBUG_VULKAN)
        VkDebugUtilsMessengerCreateInfoEXT messenger_info{};
        MakeDebugUtilsMessengerCreateInfoEXT(messenger_info, this);
        if (GET_PARAM_TYPE_VAL(BOOL, VULKAN_INSTANCE_DEBUG_UTILS_EXT)) {
            VSChainPushFront(&instance_info, &messenger_info);
        }

        VkDebugReportCallbackCreateInfoEXT report_info{};
        MakeDebugReportCallbackCreateInfoEXT(report_info, this);
        if (GET_PARAM_TYPE_VAL(BOOL, VULKAN_INSTANCE_DEBUG_REPORT_EXT)) {
            VSChainPushFront(&instance_info, &report_info);
        }
#endif 

        /*check instance extensions*/
        if (instance_info.enabledExtensionCount > 0)
        {
            uint32_t extensions_count{ 0 };
            vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &extensions_count, VK_NULL_HANDLE);
            SmallVector<VkExtensionProperties> extensions(extensions_count);
            vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &extensions_count, extensions.data());

            const auto extension_check = [&](const char* ext_name) {
                auto iter = std::find_if(extensions.begin(), extensions.end(), [&](VkExtensionProperties prop) {return strcmp(prop.extensionName, ext_name) == 0; });
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
                    auto iter = std::find_if(layers.begin(), layers.end(), [&](VkLayerProperties prop) {return strcmp(layer_name, prop.layerName) == 0; });
                    if (iter == layers.end())
                        return true;
                    return false;
                };

                if (std::any_of(instance_info.ppEnabledLayerNames, instance_info.ppEnabledLayerNames + instance_info.enabledLayerCount, layer_check))
                {
                    PLOG(ERROR) << "some layers not support by instance";
                }
            }
        }
        //create instance
        auto ret = vkCreateInstance(&instance_info, g_host_alloc_vulkan, &instance_ptr->instance_);
        if (ret != VK_SUCCESS) {
            LOG(ERROR) << "create vulkan instance failed";
        }

        //create surface
#ifdef WIN32
        {
            VkWin32SurfaceCreateInfoKHR surface_info{};
            MakeSurfaceCreateInfo(surface_info, nullptr);
            if (vkCreateWin32SurfaceKHR(instance_ptr->instance_, &surface_info, g_host_alloc_vulkan, &instance_ptr->surface_) != VK_SUCCESS) {
                LOG(ERROR) << "create win32 surface failed";
                //todo
            }
        }
#endif

        //initialize volk
        volkLoadInstanceOnly(instance_ptr->instance_);
        return instance_ptr;
    }

    VkPhysicalDevice VulkanInstance::EnumeratePhysicalDevice(const Span<const char*> extensions) const
    {
        uint32_t phy_device_count{ 0u };
        auto ret = vkEnumeratePhysicalDevices(instance_, &phy_device_count, VK_NULL_HANDLE);
        assert(VK_SUCCESS == ret && "no phy device enumerated");
        SmallVector<VkPhysicalDevice> phy_devices(phy_device_count);
        vkEnumeratePhysicalDevices(instance_, &phy_device_count, phy_devices.data());
        //find the best physical device
        const auto query_phy_device = [&](auto device)->bool {
            uint32_t extension_count;
            //check extension
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

            //check bindless feature and other features
            {
                VkPhysicalDeviceFeatures2 device_features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = VK_NULL_HANDLE };
                VkPhysicalDeviceDescriptorIndexingFeatures index_features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES};
                if (GET_PARAM_TYPE(BOOL, HAL_RESOURCE_BINDLESS)) {
                    VSChainPushFront(device_feature, indexing_features);
                }
                VkPhysicalDeviceFragmentShadingRateFeaturesKHR shading_rate_features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR };
                VkPhysicalDeviceFragmentShadingRatePropertiesKHR shading_rate_props{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR };
                if (GET_PARAM_TYPE(BOOL, HAL_SHADING_VRS) {
                    shading_rate_features.pNext = shading_rate_props;
                    VSChainPushFront(device_feature, shading_rate_features);
                }
                //todo add other feature check
                vkGetPhysicalDeviceFeatures2(device, &device_features);
                if (GET_PARAM_TYPE(BOOL, HAL_RESOURCE_BINDLESS)) {
                    if (!index_features.shaderSampledImageArrayNonUniformIndexing ||
                        !index_features.descriptorBindingSampledImageUpdateAfterBind ||
                        !index_features.shaderUniformBufferArrayNonUniformIndexing ||
                        !index_features.descriptorBindingUniformBufferUpdateAfterBind ||
                        !index_features.shaderStorageBufferArrayNonUniformIndexing ||
                        !index_features.descriptorBindingStorageBufferUpdateAfterBind) {
                        LOG(ERROR) << "vulkan index after bind is not supported while using bindless";
                        return false;
                    }
                }

                if (GET_PARAM_TYPE(BOOL, HAL_SHADING_VRS) {
                    if (!shading_rate_features.pipelineFragmentShadingRate ||
                        !shading_rate_features.primitiveFragmentShadingRate ||
                        !shading_rate_features.attachmentFragmentShadingRate) {
                        LOG(ERROR) << "vulkan vrs shading is not supported";
                    }
                    return false;
                }
            }
            
            return true;
            };
        auto selected_phy_device = eastl::find_if(phy_devices.begin(), phy_devices.end(), query_phy_device);
        if (selected_phy_device != phy_devices.end()) {
            return *selected_phy_device;
        }
        else {
            LOG(ERROR) << "vulkan can not find physic device suitable";
        }
        return VK_NULL_HANDLE;
    }

#ifdef defined(DEVELOP_DEBUG_TOOLS)&&defined(DEBUG_VULKAN)
    bool VulkanInstance::CreateDebugReportCallback(const VkDebugReportCallbackCreateInfoEXT* create_info, VkDebugReportCallbackEXT& call_back)
    {
        auto pfn_create_debug_reporter = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_ptr->handle, "vkCreateDebugReportCallbackEXT");
        if (pfn_create_debug_reporter) {
            auto result = pfn_create_debug_reporter(instance_, create_info, g_host_alloc_vulkan, &call_back);
            return result == VK_SUCCESS;
        }
        return false;
    }

    bool VulkanInstance::CreateDebugUtilsMessenger(const VkDebugUtilsMessengerCreateInfoEXT* create_info, VkDebugUtilsMessengerEXT& messager)
    {
        auto pfn_create_messager = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_ptr->handle_, "vkCreateDebugUtilsMessengerEXT");
        if (pfn_create_messager) {
            auto result = pfn_create_messager(instance_, create_info, g_host_alloc_vulkan, &messager);
            return false;
        }
        return false;
    }
#endif

    VulkanDevice::SharedPtr VulkanDevice::Create(VulkanInstance* instance)
    {
        VulkanDevice::SharedPtr device_ptr{ new VulkanDevice };
        SmallVector<const char*> extensions;
        ADD_EXT_IF(GET_PARAM_TYPE_VAL(BOOL, VULKAN_DEVICE_MEMORY_BUDGET), VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
        ADD_EXT_IF(GET_PARAM_TYPE_VAL(BOOL, VULKAN_DEVICE_MEMORY_REQUIRE), VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
        ADD_EXT_IF(GET_PARAM_TYPE_VAL(BOOL, VULKAN_DEVICE_DEDICATED_ALLOC), VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
        ADD_EXT_IF(GET_PARAM_TYPE_VAL(BOOL, VULKAN_DEVICE_DESCRIPTOR_INDEX_EXT), VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
        ADD_EXT_IF(GET_PARAM_TYPE_VAL(BOOL, VULKAN_DEVICE_BUFFER_ADDRESS_EXT), VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        auto selected_phy_device =  instance->EnumeratePhysicalDevice(extensions);

        uint32_t queue_count{ 0u };
        vkGetPhysicalDeviceQueueFamilyProperties(selected_phy_device, &queue_count, nullptr);
        SmallVector<VkQueueFamilyProperties> queue_properties(queue_count);
        vkGetPhysicalDeviceQueueFamilyProperties(selected_phy_device, &queue_count, queue_properties.data());

        SmallVector<VkDeviceQueueCreateInfo, VulkanQueue::MAX_QUEUE_TYPE_COUNT> queue_create_infos;
        {
            const auto find_queue_family = [&](VkQueueFlags flags, const uint32_t queue_count, uint32_t& family_index, bool support_present) {
                for (auto n = 0; n < queue_properties.size(); ++n) {
                    auto& prop = queue_properties[n];
                    const auto type_index = GetQueueTypeIndex(flags);
                    if ((prop.queueFlags & flags) == flags && queue_count <= prop.queueCount) {
                        if (support_present) {
                            VkBool32 test_present{ false };
                            vkGetPhysicalDeviceSurfaceSupportKHR(selected_phy_device, n, instance->surface_, &test_present);
                            if (!test_present) {
                                continue;
                            }
                        }
                        family_index = n;
                        prop.queueCount -= queue_count; //reduce queue count
                        return true;
                    }
                }
                LOG(ERROR) << fmt::format("create vulkan device failed for failing to allcate queue for flags {}.", flags);
                return false;
            };

            const auto make_queue_create_info = [&](VkQueueFlags queue_type, uint32_t queue_count, const float* priority, bool support_present=false) {
                if (queue_count > 0) {
                    uint32_t queue_family{ -1 };
                    auto result = find_queue_family(queue_type, queue_count, queue_family, support_present);
                    if (result) {
                        VkDeviceQueueCreateInfo queue_info;
                        MakeQueueCreateInfo(queue_info, queue_type, queue_family, queue_count, priority); 
                        queue_create_infos.emplace_back(queue_info);
                        VulkanQueue::RegistQueueTypeFamily(queue_type, queue_family);
                    }
                }
            };
            const auto generate_queue_priority = [](const String& str, const uint32_t queue_count)->auto {
                auto priorities = StringParameterHelper::Split(str);
                if (priorities.size() != queue_count) {
                    SmallVector<float> default_priorities(queue_count, 0.5f);
                    return default_priorities;
                }
                return priorities;
            };
            //create queue from the highest priority one  to the lowest one
            {
                VkQueueFlags queue_type{ VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT };
                const auto queue_count = GET_PARAM_TYPE_VAL(UINT, VULKAN_DEVICE_UBER_QUEUE_COUNT);
                auto queue_priority = generate_queue_priority(GET_PARAM_TYPE_VAL(STRING, VULKAN_DEVICE_UBER_QUEUE_PRIOR), queue_count);
                make_queue_create_info(queue_type, queue_count, queue_priority.data(), true);
            }
            {
                VkQueueFlags queue_type{ VK_QUEUE_GRAPHICS_BIT };
                const auto queue_count = GET_PARAM_TYPE_VAL(UINT, VULKAN_DEVICE_GFX_QUEUE_COUNT);
                auto queue_priority = generate_queue_priority(GET_PARAM_TYPE_VAL(STRING, VULKAN_DEVICE_GFX_QUEUE_PRIOR), queue_count);
                make_queue_create_info(queue_type, queue_count, queue_priority.data());
            }
            {
                VkQueueFlags queue_type{  VK_QUEUE_COMPUTE_BIT };
                const auto queue_count = GET_PARAM_TYPE_VAL(UINT, VULKAN_DEVICE_COMPUTE_QUEUE_COUNT);
                auto queue_priority = generate_queue_priority(GET_PARAM_TYPE_VAL(STRING, VULKAN_DEVICE_COMPUTE_QUEUE_PRIOR), queue_count);
                make_queue_create_info(queue_type, queue_count, queue_priority.data());
            }
            {
                VkQueueFlags queue_type{ VK_QUEUE_TRANSFER_BIT };
                const auto queue_count = GET_PARAM_TYPE_VAL(UINT, VULKAN_DEVICE_TRANSFER_QUEUE_COUNT);
                auto queue_priority = generate_queue_priority(GET_PARAM_TYPE_VAL(STRING, VULKAN_DEVICE_TRANSFER_QUEUE_PRIOR), queue_count);
                make_queue_create_info(queue_type, queue_count, queue_priority.data());
            }
        }
        VkDeviceCreateInfo device_info;
        MakeDeviceCreateInfo(device_info, 0x0, queue_create_infos, extensions);
        //enable descriptor indexing feature
        VkPhysicalDeviceDescriptorIndexingFeatures indexing_features;
        if (GET_PARAM_TYPE_VAL(BOOL, HAL_RESOURCE_BINDLESS)) {
            MakeBindlessDeviceDescriptorIndexingFeatures(indexing_features);
            VSChainPushFront(&device_info, &indexing_features);
        }
        VkPhysicalDeviceFragmentShadingRateFeaturesKHR shading_features;
        if (GET_PARAM_TYPE_VAL(BOOL, HAL_SHADING_VRS)) {
            MakeFragmentVRSFeature(shading_features);
            VSChainPushFront(&device_info, &shading_features);
        }
#if defined(DEVELOP_DEBUG_TOOLS) && VK_EXT_host_query_reset
        {
            //enable query pool reset
            VkPhysicalDeviceHostQueryResetFeaturesEXT reset_query_features;
            reset_query_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES;
            reset_query_features.pNext = VK_NULL_HANDLE:
            reset_query_features.hostQueryReset = VK_TRUE;
            VSChainPushFront(&device_info, &reset_query_features);
        }
#endif
        auto result = vkCreateDevice(selected_phy_device, &device_info, g_host_alloc_vulkan, &device_ptr->device_);
        PCHECK(VK_SUCCESS == result) << "vulkan create logic device failed";

        //load device functions
        volkLoadDevice(device_ptr->device_);

        return device_ptr;
    }

    VulkanQueue VulkanDevice::GetQueue(VkQueueFlags queue_type)
    {
        //todo transform queue_type to queue_
        VkQueue queue;
        uint32_t family{ 0u }, index{ 0u };
        family = VulkanQueue::GetTypeFamilyIndex(queue_type);
        if (VulkanQueue::CreateQueueForFamily(family, index)){
            vkGetDeviceQueue(device_, family, index, &queue);
        }
        return VulkanQueue::Create(queue, queue_type, family, index);
    }

    VkSampleCountFlags VulkanDevice::GetMaxUsableSampleCount() const
    {
        //todo
        VkSampleCountFlags counts = Utils::Min(back_end_properties_.limits.framebufferColorSampleCounts,
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

    bool VulkanDevice::IsFormatSupported(VkFormat format)const
    {
        VkImageFormatProperties format_prop{};
        /*If format is not a supported image format, or if the combination of format, type, tiling, usage, and
            flags is not supported for images, then vkGetPhysicalDeviceImageFormatProperties returns VK_ERROR_FORMAT_NOT_SUPPORTED.*/
        auto ret = vkGetPhysicalDeviceImageFormatProperties(phy_device_, format, VK_IMAGE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, 0, &format_prop);
        return ret != VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    void VulkanDevice::GetFormatProperty(VkFormat format, VkFormatProperties& format_props) const
    {
        vkGetPhysicalDeviceFormatProperties(phy_device_, format, &format_props);
    }

    bool VulkanDevice::CreateCommandPool(const VkCommandPoolCreateInfo& create_info, VkCommandPool& pool)
    {
        if (VK_SUCCESS == vkCreateCommandPool(device_, &create_info, g_host_alloc_vulkan, &pool)) {
            return true;
        }
        return false;
    }

    void VulkanDevice::ResetCommandPool(VkCommandPool pool)
    {
        //set VkCommandPoolResetFlags to zero, to keep resource while doing reset
        vkResetCommandPool(device_, pool, 0u);
    }

    void VulkanDevice::DestroyCommandPool(VkCommandPool pool)
    {
        vkDestroyCommandPool(device_, pool, g_host_alloc_vulkan);
    }
    
    void VulkanDevice::WaitIdle() const
    {
        vkDeviceWaitIdle(device_);
    }

    VulkanDevice::~VulkanDevice()
    {
        if (VK_NULL_HANDLE != device_)
        {
            WaitIdle();
            vkDestroyDevice(device_, g_host_alloc_vulkan);
        }
    }

    void VulkanDevice::GetPhysicalDeviceProperties(VkPhysicalDeviceProperties& props)const
    {
        vkGetPhysicalDeviceProperties(phy_device_, &props);
    }

    void VulkanDevice::GetBufferMemoryRequirements(VkBuffer buffer, VkMemoryRequirements& requirements)const
    {
        vkGetBufferMemoryRequirements(device_, buffer, &requirements);
    }

    void VulkanDevice::GetImageMemoryRequirements(VkImage image, VkMemoryRequirements& requirements)const
    {
        vkGetImageMemoryRequirements(device_, image, &requirements);
    }

    void VulkanDevice::GetPhysicalDeviceProperties2(VkPhysicalDeviceProperties2& props2)const
    {
        vkGetPhysicalDeviceProperties2(phy_device_, &props2);
    }

    void VulkanDevice::GetPhysicalDeviceMemoryProperties2(VkPhysicalDeviceMemoryProperties2& mem_props2)const
    {
        vkGetPhysicalDeviceMemoryProperties2(phy_device_, &mem_props2);
    }

    void VulkanDevice::GetBufferMemoryRequirements2(const VkBufferMemoryRequirementsInfo2& require_info2, VkMemoryRequirements2& requirements)const
    {
        vkGetBufferMemoryRequirements2(device_, &require_info2, &requirements);
    }

    void VulkanDevice::GetImageMemoryRequirements2(const VkImageMemoryRequirementsInfo2& require_info2, VkMemoryRequirements2& requirements)const
    {
        vkGetImageMemoryRequirements2(device_, &require_info2, &requirements);
    }

    void VulkanDevice::AllocMemory(VkDeviceMemory& memory, VkMemoryAllocateInfo& alloc_info)
    {
        vkAllocateMemory(device_, &alloc_info, g_host_alloc_vulkan, &memory);
    }

    void VulkanDevice::GetSurfaceCapabilities2(VkSurfaceKHR surface, VkSurfaceCapabilities2KHR& surface_caps) const
    {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phy_device_, surface, &surface_caps);
    }

    void VulkanDevice::DeAllocMemory(VkDeviceMemory memory)
    {
        vkFreeMemory(device_, memory, g_host_alloc_vulkan);
    }

    void* VulkanDevice::MapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags)
    {
        void* mapped_data{ nullptr };
        vkMapMemory(device_, memory, offset, (size ? size : VK_WHOLE_SIZE), flags, &mapped_data);
        return mapped_data;
    }

    void VulkanDevice::UnMapMemory(VkDeviceMemory memory)
    {
        vkUnmapMemory(device_, memory);
    }

    bool VulkanDevice::CreateImage(const VkImageCreateInfo* create_info, VkImage& image)
    {
        auto result = vkCreateImage(device_, create_info, g_host_alloc_vulkan, &image); 
        return result == VK_SUCCESS;
    }

    bool VulkanDevice::CreateBuffer(const VkBufferCreateInfo* create_info, VkBuffer& buffer)
    {
        auto result = vkCreateBuffer(device_, create_info, g_host_alloc_vulkan, &buffer);
        return result == VK_SUCCESS;
    }

    void VulkanDevice::BindBufferMemory(VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset)
    {
        vkBindBufferMemory(device_, buffer, memory, offset);
    }

    void VulkanDevice::DestroyImage(VkImage image)
    {
        vkDestroyImage(device_, image, g_host_alloc_vulkan);
    }

    void VulkanDevice::DestroyBuffer(VkBuffer buffer)
    {
        vkDestroyBuffer(device_, buffer, g_host_alloc_vulkan);
    }

    bool VulkanDevice::CreateImageView(const VkImageViewCreateInfo* create_info, VkImageView& image_view)
    {
        auto result = vkCreateImageView(device_, create_info, g_host_alloc_vulkan, &image_view);
        return result == VK_SUCCESS;
    }

    void VulkanDevice::BindImageMemory(VkImage image, VkDeviceMemory memory, VkDeviceSize offset)
    {
        vkBindImageMemory(device_, image, memory, offset);
    }

    void VulkanDevice::DestroyImageView(VkImageView view)
    {
        vkDestroyImageView(device_, view, g_host_alloc_vulkan);
    }

    bool VulkanDevice::CreateFrameBuffer(const VkFramebufferCreateInfo* create_info, VkFramebuffer& frame_buffer)
    {
        auto result = vkCreateFramebuffer(device_, create_info, g_host_alloc_vulkan, &frame_buffer);
    }

    void VulkanDevice::DestroyFrameBuffer(VkFramebuffer frame_buffer)
    {
        vkDestroyFramebuffer(device_, frame_buffer, g_host_alloc_vulkan);
    }

    bool VulkanDevice::CreateEvents(const VkEventCreateInfo* event_info, VkEvent& event)
    {
        return vkCreateEvent(device_, event_info, g_host_alloc_vulkan, &event) == VK_SUCCESS;
    }

    void VulkanDevice::DestroyEvents(VkEvent event)
    {
        vkDestroyEvent(device_, event, g_host_alloc_vulkan);
    }

    bool VulkanDevice::CreateSemaphore(const VkSemaphoreCreateInfo* create_info, VkSemaphore& semaphore)
    {
        return vkCreateSemaphore(device_, create_info, g_host_alloc_vulkan, &semaphore);
    }

    void VulkanDevice::DestroySemaphore(VkSemaphore semaphore)
    {
        vkDestroySemaphore(device_, semaphore, g_host_alloc_vulkan);
    }

    void VulkanDevice::SignalSemaphore(const VkSemaphoreSignalInfo& signal_info)
    {
        vkSignalSemaphore(device_, &signal_info);
    }

    void VulkanDevice::WaitSemaphore(const VkSemaphoreWaitInfo& wait_info, uint64_t time_out)
    {
        vkWaitSemaphores(device_, &wait_info, time_out); 
    }

    void VulkanDevice::GetSemaphoreCounterValue(VkSemaphore semaphore, uint64_t curr_value)
    {
        vkGetSemaphoreCounterValue(device_, semaphore, &curr_value);
    }

    bool VulkanDevice::CreateFence(const VkFenceCreateInfo* fence_info, VkFence& fence)
    {
        return vkCreateFence(device_, fence_info, g_host_alloc_vulkan, &fence);
    }

    void VulkanDevice::DestroyFence(VkFence fence)
    {
        vkDestroyFence(device_, fence, g_host_alloc_vulkan);
    }

    bool VulkanDevice::CreateRenderPass(const VkRenderPassCreateInfo* pass_info, VkRenderPass& pass)
    {
        return vkCreateRenderPass(device_, pass_info, g_host_alloc_vulkan, &pass);
    }

    void VulkanDevice::DestroyRenderPass(VkRenderPass pass)
    {
        vkDestroyRenderPass(device_, pass, g_host_alloc_vulkan);
    }

    bool VulkanDevice::CreateFramebuffer(const VkFramebufferCreateInfo* fbo_info, VkFramebuffer& fbo)
    {
        return vkCreateFramebuffer(device_, fbo_info, g_host_alloc_vulkan, &fbo);
    }

    void VulkanDevice::DestroyFramebuffer(VkFramebuffer fbo)
    {
        vkDestroyFramebuffer(device_, fbo, g_host_alloc_vulkan);
    }

#ifdef DEVELOP_DEBUG_TOOLS
    bool VulkanDevice::CreateQueryPool(const VkQueryPoolCreateInfo* pool_info, VkQueryPool& pool)
    {
        return vkCreateQueryPool(device_, pool_info, g_host_alloc_vulkan, &pool);
    }
    void VulkanDevice::DestroyQueryPool(VkQueryPool pool)
    {
        vkDestroyQueryPool(device_, pool, g_host_alloc_vulkan);
    }
    void VulkanDevice::ResetQueryPool(VkQueryPool pool, uint32_t first_query, uint32_t query_count)
    {
        vkResetQueryPool(device_, pool, first_query, query_count);
    }
    bool VulkanDevice::GetQueryPoolResults(VkQueryPool pool, uint32_t first_query, uint32_t query_count, size_t data_size, void* data, VkDeviceSize stride, VkQueryResultFlags flags)
    {
        return VK_SUCCESS == vkGetQueryPoolResults(device_, pool, first_query, query_count, data_size, data, stride, flags);
    }
#endif

    uint32_t VulkanQueue::GetTypeFamilyIndex(VkQueueFlags type)
    {
        const auto type_index = GetQueueTypeIndex(type);
        return queue_type_family_[type_index];
    }

    bool VulkanQueue::CreateQueueForFamily(uint32_t family_index, uint32_t& queue_index)
    {
        queue_index = family_queue_count_[family_index]--;
        return queue_index >= 0u;
    }

    void VulkanQueue::RegistQueueTypeFamily(VkQueueFlags queue_type, uint32_t family_index)
    {
        const auto type_index = GetQueueTypeIndex(queue_type);
        queue_type_family_[type_index] = family_index;
    }

    void VulkanQueue::RegistQueueFamilyCount(uint32_t family_index, uint32_t queue_count)
    {
        family_queue_count_[family_index] = queue_count;
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
        vkQueueSubmit(queue_, 1, &submit_info, fence); 
    }

    void VulkanQueue::Submit(const VkPresentInfoKHR& present_info)
    {
        vkQueuePresentKHR(queue_, &present_info);
    }

    void VulkanQueue::WaitIdle()const
    {
        vkQueueWaitIdle(queue_);
    }
}