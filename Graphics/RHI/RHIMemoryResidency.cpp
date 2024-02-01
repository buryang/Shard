#include "RHIMemoryResidency.h"

namespace Shard::RHI
{
    std::atomic_uint32_t g_allocation_counter{ 0u };
}
