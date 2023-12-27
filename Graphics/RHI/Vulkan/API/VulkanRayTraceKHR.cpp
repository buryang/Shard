#include "VulkanRayTraceKHR.h"

namespace Shard
{

        /*return bind table information*/
        const VulkanRayTraceBindTable::Table& VulkanRayTraceBindTable::GetBindTable() const
        {
                return shader_tbl_;
        }
}