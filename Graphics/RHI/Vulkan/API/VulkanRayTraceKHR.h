#pragma once
#include "VulkanRHI.h"

namespace Shard
{
    namespace Internal {
        class VulkanVMAMemAllocator;
    }
    class VulkanSceneAccStructure
    {
    public:
        VkAccelerationStructureKHR Get() {
            return scene_;
        }
        void Build();
        void Update();
    private:
        void BuildTLAS();
        void UpdateTLAS();
        void BuildBLAS();
        void UpdateBLAS();
    private:
        VkAccelerationStructureKHR            scene_{ VK_NULL_HANDLE };
        Internal::VulkanVMAMemAllocator*    allocator_{nullptr};
    };

    class VulkanRayTraceBindTable
    {
    public:
        using Table        = VkStridedDeviceAddressRegionKHR;
        using TableImpl = VkBuffer;
        enum class Type:uint32_t
        {
            eRegen,
            eRmiss,
            eRhit,
            eRcall,
            eShaderGroupCount,
            eCount
        };

        struct HitEntry
        {
            uint32_t instance_offset_{ 0 };
            uint32_t geometry_index_{ 0 };
            uint32_t sbt_offset_{ 0 };
            uint32_t sbt_stride_{ 0 };

        };
        struct _Entry
        {
            uint32_t stride_{ 0 };
            uint32_t index_{ 0 };
        };

        using MissEntry = _Entry;
        using CallEntry = _Entry;

        VulkanRayTraceBindTable() = default;
        VulkanRayTraceBindTable(const String& spv_file);
        const Table& GetBindTable()const;
    private:
        //void Compile(const String& glsl_file);
        void Load(const String& spv_file);
    private:
        Table                    shader_tbl_;
        TableImpl                shader_tbl_impl_{ VK_NULL_HANDLE };
        VkDeviceSize            offset_;
        VkDeviceSize            stride_;
        VkDeviceSize            size_;
        Type                    type_;
        Vector<HitEntry>        hit_entries_;
        Vector<MissEntry>        miss_entries_;
    };




}