#pragma once
#include "Utils/CommonUtils.h"
#include "Core/EngineGlobalParams.h"
#include "RHI/RHIResources.h"
#include "RHI/Vulkan/API/VulkanRHI.h"
#include <vulkan/vulkan_core.h>

namespace Shard::RHI::Vulkan
{
        enum
        {
                RHI_HEAP0_PER_BLOCK_ALLOC_SIZE = 256 * 1024 * 1024,
                RHI_HEAP1_PER_BLOCK_ALLOC_SIZE = 64 * 1024 * 1024,
        };

        //set max pooled resource count
        REGIST_PARAM_TYPE(UINT, RHI_POOLED_MAX_RES, 256);
        REGIST_PARAM_TYPE(FLOAT, RHI_POOLED_TIME_GAP, 12);

        struct MemoryBulkDesc
        {
                VkDeviceSize        size_{ 0 };
                uint32_t        mem_type_;
        };

        struct MemoryAllocRequirements
        {
                enum class EAllocBindType
                {
                        eUnDefined,
                        eBuffer,
                        eImageUnkown,
                        eImageLinear,
                        eImageOptimal,
                        eNum,
                };
                EAllocBindType        bind_type_{};
                //todo alignment vs offset requirements
                VkDeviceSize        alignment_{ 0 };
                VkDeviceSize        size_{ 0 };
                uint32_t        mem_type_;
        };

        /*
         *memoryOffset must be an integer multiple of the alignment member of the
         *VkMemoryRequirements structure returned from a call to vkGetBufferMemoryRequirements with buffer
        */

        struct MemoryAllocation
        {
                enum class EType : uint8_t
                {
                        eNone,
                        eDedicated,
                        eBulk,
                };
                EType        type_{ EType::eNone };
                VkDeviceMemory        mem_{ VK_NULL_HANDLE };
                VkDeviceSize        offset_{ 0 };
                VkDeviceSize        size_{ 0 };
                //parent bulk
                RHIHeapMemoryBulkVulkanAllocator::Ptr parent_{ nullptr };
                FORCE_INLINE bool IsValid() const {
                        return EType::eNone != type_;
                }
        };

        void ReleaseMemoryAllocation(MemoryAllocation& memory);

        class RHIDeviceMemoryStateCounter
        {
        public:
                static RHIDeviceMemoryStateCounter& Instance() {
                        static RHIDeviceMemoryStateCounter instance;
                        return instance;
                }
                void Init(float heap_mem_percent);
                void UnInit();
                void Tick(float time);
                VkDeviceSize GetTotalMemorySize(bool is_gpu) const;
                uint32_t GetHeapIndexForMemType(uint32_t mem_type) const;
                VkDeviceSize GetHeapBudget(uint32_t heap_index) const;
                VkDeviceSize GetHeapUsage(uint32_t heap_index) const;
                VkDeviceSize GetHeapPeakUsage(uint32_t heap_index) const;
                uint32_t GetAllocCount() const;
                bool InCreAllocCount();
        private:
                RHIDeviceMemoryStateCounter() = default;
                DISALLOW_COPY_AND_ASSIGN(RHIDeviceMemoryStateCounter);
        private:
                struct HeapInfo
                {
                        VkDeviceSize        total_size_{ 0 };
                        VkDeviceSize        used_size_{ 0 };
                        VkDeviceSize        peak_size_{ 0 };
                };
                SmallVector<HeapInfo, VK_MAX_MEMORY_HEAPS, false>        heap_infos_;
                VkPhysicalDeviceMemoryProperties        mem_props_;
                VkPhysicalDeviceMemoryBudgetPropertiesEXT        mem_budget_;
                //plan used budget size
                float mem_percent_{ .95f };
                uint32_t alloc_count_{ 0 };
                uint32_t max_alloc_count_{ 0 };
        };

        class RHIHeapMemoryBulkVulkanAllocator
        {
        public:
                using Ptr = RHIHeapMemoryBulkVulkanAllocator*;
                using SharedPtr = eastl::shared_ptr<RHIHeapMemoryBulkVulkanAllocator>;
                enum class ERecycleType {
                        eUnkown = 0x0,
                        eEvitAble = 0x1,
                        eDefragAble = 0x2,
                        eBoth = eEvitAble | eDefragAble,
                };
                explicit RHIHeapMemoryBulkVulkanAllocator(const MemoryBulkDesc& desc);
                virtual ~RHIHeapMemoryBulkVulkanAllocator() { Release(); }
                virtual MemoryAllocation Alloc(const MemoryAllocRequirements& alloc_require) = 0;
                virtual void Free(MemoryAllocation mem) = 0;
                virtual void Tick(float time) = 0;
                virtual bool IsFull()const = 0;
                virtual void Release() {
                        vkFreeMemory(GetGlobalDevice(), mem_bulk_, g_host_alloc);
                        mem_bulk_ = VK_NULL_HANDLE;
                }
                virtual ERecycleType GetRecycleType() const {
                        return ERecycleType::eUnkown;
                }
                FORCE_INLINE uint32_t GetHeapType() const {
                        return type_;
                }
                FORCE_INLINE VkDeviceMemory GetBulkMemory() const {
                        return mem_bulk_;
                }
                FORCE_INLINE VkDeviceSize GetBulkSize() const {
                        return capacity_;
                }
        protected:
                DISALLOW_COPY_AND_ASSIGN(RHIHeapMemoryBulkVulkanAllocator);
        private:
                const uint32_t        type_;
                const VkDeviceSize        capacity_;
                VkDeviceMemory        mem_bulk_{ VK_NULL_HANDLE };
        };

        class RHIReuseHeapMemoryBulkVulkanAllocator : public RHIHeapMemoryBulkVulkanAllocator
        {
        public:
                using SharedPtr = eastl::shared_ptr<RHIReuseHeapMemoryBulkVulkanAllocator>;
                struct Range {
                        uint32_t        offset_ : 16{0};
                        uint32_t        size_ : 16{0};
                        void* meta_data_{ nullptr };
                        FORCE_INLINE bool IsValid() const {
                                return offset_ >= 0 && size_ > 0;
                        }
                        template<typename MetaType>
                        MetaType* GetMeta() {
                                return static_cast<MetaType*>(meta_data_);
                        }
                };
                RHIReuseHeapMemoryBulkVulkanAllocator(const MemoryBulkDesc& desc) : RHIHeapMemoryBulkVulkanAllocator(desc) {
                        free_blks_.emplace_back(Range{.size_ = GetBulkSize()});
                }
                MemoryAllocation Alloc(const MemoryAllocRequirements& alloc_require) override;
                bool IsFull()const override {
                        return free_blks_.empty();
                }
                void Free(MemoryAllocation mem) override;
                void Tick(float time) override;
        private:
                bool FindSuitAbleFreeRange(uint32_t size, Range& range);
                bool FreeRangesSanityCheck() const;
                bool InsertAndMergeFreeRange(List<Range>& range_list, Range& range);
        private:
                /*<float:last_time_stamp, Range:range>*/
                List<eastl::pair<float, Range>>        used_blks_;
                List<Range>        free_blks_;
                float curr_time_stamp_{ 0 };
        };

        class RHIRingHeapMemoryBulkVulkanAllocator : public RHIHeapMemoryBulkVulkanAllocator
        {
        public:
                RHIRingHeapMemoryBulkVulkanAllocator(const MemoryBulkDesc& desc) : RHIHeapMemoryBulkVulkanAllocator(desc) {}
                MemoryAllocation Alloc(const MemoryAllocRequirements& alloc_require) override;
                void Free(MemoryAllocation mem) override;
                void Tick(float time) override;
                bool IsFull() const override {
                        return head_ >= tail_ + GetBulkSize();
                }
        private:
                VkDeviceSize        head_{ 0 };
                VkDeviceSize        tail_{ 0 };
        };

        class RHILinearHeapMemoryBulkVulkanAllocator : public RHIHeapMemoryBulkVulkanAllocator
        {
        public:
                RHILinearHeapMemoryBulkVulkanAllocator(const MemoryBulkDesc& desc) : RHIHeapMemoryBulkVulkanAllocator(desc) {}
                MemoryAllocation Alloc(const MemoryAllocRequirements& alloc_require) override;
                void Free(MemoryAllocation mem) override;
                void Tick(float time) override;
                bool IsFull() const override {
                        return  curr_offset_ >= GetBulkSize();
                }
        private:
                VkDeviceSize        curr_offset_{ 0 };
        };

        class RHILinearHeapMemoryBulkVulkanSubAllocator
        {
        public:
                using Ptr = RHILinearHeapMemoryBulkVulkanSubAllocator*;
                using SharedPtr = eastl::shared_ptr<RHILinearHeapMemoryBulkVulkanSubAllocator>;

                MemoryAllocation Alloc(const MemoryAllocRequirements& alloc_require);
                void Free(MemoryAllocation mem);
                void Tick(float time);
        private:
                friend class RHIPooledTextureAllocatorVulkan;
                Map<uint32_t, SmallVector<RHILinearHeapMemoryBulkVulkanAllocator::SharedPtr>>        mem_heaps_;
        };

        //only big texture need pooled big memory chunk
        class RHIPooledTextureAllocatorVulkan
        {
        public:
                using Ptr = RHIPooledTextureAllocatorVulkan*;
                using SharedPtr = eastl::shared_ptr<RHIPooledTextureAllocatorVulkan>;
                RHIPooledTextureAllocatorVulkan() {
                        if (!GET_PARAM_TYPE_VAL(BOOL, DEVICE_DEDICATED_ALLOC)) {
                                alloc_.reset(new RHILinearHeapMemoryBulkVulkanSubAllocator);
                        }
                }
                bool AllocBuffer(const RHIBufferInitializer& desc, RHIBufferVulkan::Ptr buffer);
                bool AllocTexture(const RHITextureInitializer& desc, RHITextureVulkan::Ptr texture);
                void Free(RHIResource::Ptr resource);
                bool Tick(float time);
        private:
                struct PooledResource
                {
                        RHITextureInitializer        texture_desc_;
                        RHIBufferInitializer        buffer_desc_;
                        RHIResource::Ptr        handle_{ nullptr };
                        uint32_t        last_stamp_{ 0 };
                        operator RHIResource::Ptr() {
                                return handle_;
                        }
                        uint32_t LastStamp()const {
                                return last_stamp_;
                        }
                        ~PooledResource() {
                                if (nullptr != handle_) {
                                        delete handle_;
                                }
                        }
                };
                List<PooledResource>        pooled_repo_;
                float curr_time_stamp_{ 0 };
                RHILinearHeapMemoryBulkVulkanSubAllocator::SharedPtr alloc_;
        private:
                bool FindSuitAbleResource(bool is_texture, const void* desc, PooledResource& ret);
        };

        class RHITransientResourceAllocatorVulkan
        {
        public:
                using Ptr = RHITransientResourceAllocatorVulkan*;
                using SharedPtr = eastl::shared_ptr<RHITransientResourceAllocatorVulkan>;
                RHIResource::Ptr AllocTexture(const RHITextureInitializer& desc);
                RHIResource::Ptr AllocBuffer(const RHIBufferInitializer& desc);
                void Free(RHIResource::Ptr resource);
                void Tick(float time);
                ~RHITransientResourceAllocatorVulkan();
        private:
                //<mem type index, allocator>
                Map<uint32_t, RHIReuseHeapMemoryBulkVulkanAllocator::SharedPtr> alloc_;
                float curr_time_stamp_{ 0 };
        };
}
