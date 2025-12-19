#pragma once

namespace Shard::Renderer
{
    //global renderer allocator
    auto& GetRendererAllocator() {
        return TLSScalablePoolAllocatorInstance<uint8_t, POOL_RENDER_SYSTEM_ID>;
    }

    template<typename T>
    using RenderArray = Vector<T, Allocator = GetRendererAllocator()>;

}
