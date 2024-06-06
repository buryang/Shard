#pragma once
#include "Utils/CommonUtils.h"
#include "HAL/Vulkan/HALGlobalEntityVulkan.h"
#include "volk/volk.h"
#include "glfw/glfw3.h"

namespace Shard
{
    extern VkAllocationCallbacks* g_host_alloc_vulkan;
    /**
     * \brief deal with all app parameters config verfiy and instance operation work
     */
    class VulkanInstance
    {
    public:
        using SharedPtr = std::shared_ptr<VulkanInstance>;
        static SharedPtr Create(const Span<const char*>& require_extensions);
        VkPhysicalDevice EnumeratePhysicalDevice(const Span<const char*> extensionss)const;
#ifdef defined(DEVELOP_DEBUG_TOOLS)&&defined(DEBUG_VULKAN)
        bool CreateDebugReportCallback(const VkDebugReportCallbackCreateInfoEXT* create_info, VkDebugReportCallbackEXT& call_back);
        bool CreateDebugUtilsMessenger(const VkDebugUtilsMessengerCreateInfoEXT* create_info, VkDebugUtilsMessengerEXT& messager);
#endif
        ~VulkanInstance() { 
            if (instance_!=VK_NULL_HANDLE) { 
                vkDestroyInstance(instance_, g_host_alloc_vulkan);
            } 
        }
    private:
        
        DISALLOW_COPY_AND_ASSIGN(VulkanInstance);
        OVERLOAD_OPERATOR_NEW(VulkanInstance);
        VkInstance  instance_{ VK_NULL_HANDLE };
        VkSurfaceKHR    surface_{ VK_NULL_HANDLE };
    };

    //todo alloc queue, and do swapchain init check
    class VulkanQueue;
    class VulkanCmdPoolManager;

    class VulkanDevice 
    {
    public:
        using SharedPtr = std::shared_ptr<VulkanDevice>;
        friend class VulkanInstance;
        /**
         * \brief create device from instance
         * \param instance
         * \return 
         */
        static SharedPtr Create(VulkanInstance* instance);
        FORCE_INLINE bool IsUMA() const { return is_uma_; }
        VulkanQueue GetQueue(VkQueueFlags queue_type);
        //function to deal with command buffer logic
        VulkanCmdPoolManager& GetCmdPoolManager() { return pool_manager_; }
        void WaitIdle()const;
        ~VulkanDevice();
        void GetPhysicalDeviceProperties(VkPhysicalDeviceProperties& props)const;
        void GetBufferMemoryRequirements(VkBuffer buffer, VkMemoryRequirements& requirements)const;
        void GetImageMemoryRequirements(VkImage image, VkMemoryRequirements& requirements)const;
        bool IsFormatSupported(VkFormat format)const;
        void GetFormatProperty(VkFormat format, VkFormatProperties& format_props)const;
        bool CreateCommandPool(const VkCommandPoolCreateInfo& create_info, VkCommandPool& pool);
        void ResetCommandPool(VkCommandPool pool);
        void DestroyCommandPool(VkCommandPool pool);
        //todo how to check whether enable VK_KHR_get_memory_requirements2
#if VK_KHR_get_memory_requirements2
        void GetPhysicalDeviceProperties2(VkPhysicalDeviceProperties2& props2)const;
        void GetPhysicalDeviceMemoryProperties2(VkPhysicalDeviceMemoryProperties2& mem_props2)const;
        void GetBufferMemoryRequirements2(const VkBufferMemoryRequirementsInfo2& require_info2, VkMemoryRequirements2& requirements)const;
        void GetImageMemoryRequirements2(const VkImageMemoryRequirementsInfo2& require_info2, VkMemoryRequirements2& requirements)const;
#endif
        void GetSurfaceCapabilities2(VkSurfaceKHR surface, VkSurfaceCapabilities2KHR& surface_caps)const;
        void AllocMemory(VkDeviceMemory& memory, VkMemoryAllocateInfo& alloc_info);
        void DeAllocMemory(VkDeviceMemory memory);
        void* MapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags);
        void UnMapMemory(VkDeviceMemory memory);

        bool CreateBuffer(const VkBufferCreateInfo* create_info, VkBuffer& buffer);
        void DestroyBuffer(VkBuffer buffer);
        void BindBufferMemory(VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset);
        bool CreateImage(const VkImageCreateInfo* create_info, VkImage& image);
        void DestroyImage(VkImage image);
        void BindImageMemory(VkImage image, VkDeviceMemory memory, VkDeviceSize offset);
        bool CreateImageView(const VkImageViewCreateInfo* create_info, VkImageView& image_view);
        void DestroyImageView(VkImageView view);
        
        bool CreateFrameBuffer(const VkFramebufferCreateInfo* create_info, VkFramebuffer& frame_buffer);
        void DestroyFrameBuffer(VkFramebuffer frame_buffer);

        //sync resource function
        bool CreateEvents(const VkEventCreateInfo* event_info, VkEvent& event);
        void DestroyEvents(VkEvent event);
        bool CreateSemaphore(const VkSemaphoreCreateInfo* semaphore_info, VkSemaphore& semaphore);
        void DestroySemaphore(VkSemaphore semaphore);
        void SignalSemaphore(const VkSemaphoreSignalInfo& signal_info);
        void WaitSemaphore(const VkSemaphoreWaitInfo& wait_info, uint64_t time_out);
        void GetSemaphoreCounterValue(VkSemaphore semaphore, uint64_t curr_value);
        bool CreateFence(const VkFenceCreateInfo* fence_info, VkFence& fence);
        void DestroyFence(VkFence fence);

        //render pass function
        bool CreateRenderPass(const VkRenderPassCreateInfo* pass_info, VkRenderPass& pass);
        void DestroyRenderPass(VkRenderPass pass);
        bool CreateFramebuffer(const VkFramebufferCreateInfo* fbo_info, VkFramebuffer& fbo);
        void DestroyFramebuffer(VkFramebuffer fbo);

        //timestamp
#ifdef DEVELOP_DEBUG_TOOLS
        bool CreateQueryPool(const VkQueryPoolCreateInfo* pool_info, VkQueryPool& pool);
        void DestroyQueryPool(VkQueryPool pool);
        void ResetQueryPool(VkQueryPool pool, uint32_t first_query, uint32_t query_count);
        bool GetQueryPoolResults(VkQueryPool pool, uint32_t first_query, uint32_t query_count, size_t data_size, void* data, VkDeviceSize stride, VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
#endif
    private:
        VulkanDevice() = default;
        DISALLOW_COPY_AND_ASSIGN(VulkanDevice);
        OVERLOAD_OPERATOR_NEW(VulkanDevice);
    private:
        VkDevice    device_{ VK_NULL_HANDLE };
        VkPhysicalDevice    phy_device_{ VK_NULL_HANDLE };
        VulkanCmdPoolManager    pool_manager_;

        uint32_t    is_uma_ : 1;

    };

    class VulkanCmdBuffer;
    class VulkanQueue
    {
    public:
        friend class VulkanDevice;
        static constexpr auto MAX_QUEUE_TYPE_COUNT = 4u;
        static constexpr auto MAX_QUEUE_FAMILY_COUNT = 8u; //my card only has 3 family
        static uint32_t GetTypeFamilyIndex(VkQueueFlags type);
        static bool CreateQueueForFamily(uint32_t family_index, uint32_t& queue_index);
        static void RegistQueueTypeFamily(VkQueueFlags queue_type, uint32_t family_index);
        static void RegistQueueFamilyCount(uint32_t family_index, uint32_t queue_count);
        static VulkanQueue Create(VkQueue queue, VkQueueFlags type, uint32_t family, uint32_t index) { return { queue, type, family, index }; }
        void Submit(VulkanCmdBuffer::SharedPtr cmd_buffers, const Span<VkSemaphore>& wait_semaphores, const Span<VkSemaphore>& signal_semaphores, VkFence fence=VK_NULL_HANDLE);
        void Submit(const VkPresentInfoKHR& present_info);
        void WaitIdle()const;
        uint32_t GetFamilyIndex()const { return family_index_; }
    private:
        VulkanQueue(VkQueue queue, VkQueueFlags type, uint32_t family, uint32_t index) :queue_(queue), type_(type), family_index_(family), queue_index_(index) {}
    private:
        VkQueue queue_{ VK_NULL_HANDLE };
        VkQueueFlags    type_{ 0u };
        uint32_t    family_index_{ -1 };
        uint32_t    queue_index_{ -1 };
        static uint32_t queue_type_family_[MAX_QUEUE_TYPE_COUNT];
        static uint32_t family_queue_count_[MAX_QUEUE_FAMILY_COUNT];
    };

    class VulkanDeviceObject
    {
    public:
        explicit VulkanDeviceObject(VulkanDevice::SharedPtr parent) :parent_(parent) {}
        VulkanDevice::SharedPtr GetParent()const { return parent_; }
        uint32_t GetParentMask() const { return mask_; } //todo return device mask
        VkEvent& GetEvent() { return event_; }
        VkSemaphore& GetGFXSemaphore() { return gfx_semaphore_; }
        VkSemaphore& GetAsyncSemaphore() { return async_semaphore_; }
        bool IsSyncByEvent()const { return event_ != VK_NULL_HANDLE; }
        bool IsSyncBySemhaphore()const { return gfx_semaphore_ != VK_NULL_HANDLE || async_semaphore_ != VK_NULL_HANDLE; }
        /**
         * \return underlying vulkan handle
         */
        virtual void* GetHandle() = 0;
        virtual ~VulkanDeviceObject() {
            if (event_ != VK_NULL_HANDLE) {
                parent_->DestroyEvents(event_);
                event_ = VK_NULL_HANDLE;
            }
            if (gfx_semaphore_ != VK_NULL_HANDLE) {
                parent_->DestroySemaphore(gfx_semaphore_);
                gfx_semaphore_ = VK_NULL_HANDLE;
            }
            if (async_semaphore_ != VK_NULL_HANDLE) {
                parent_->DestroySemaphore(async_semaphore_);
                async_semaphore_ = VK_NULL_HANDLE;
            }
        }
    protected:
        VulkanDevice::SharedPtr parent_;
        VkEvent event_{ VK_NULL_HANDLE };
        /*for semaphore can be wait for once, one semaphore for gfx queue and one for async queue*/
        VkSemaphore gfx_semaphore_{ VK_NULL_HANDLE };
        VkSemaphore async_semaphore_{ VK_NULL_HANDLE };
        uint32_t    mask_{ 0u };
    };

    /**
     * \brief vulkan chain structure next parameter altogether
     * \param mains main struct
     * \param news new struct to chain
     */
    template<typename MainT, typename NewT>
    inline void VSChainPushFront(MainT* mains, NewT* news) {
        news->pNext = std::exchange(mains->pNext, news);
    }
}

