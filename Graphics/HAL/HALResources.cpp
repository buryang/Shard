#include "HAL/HALTypeTraits.h"
#include "HAL/HALResources.h"

namespace Shard::HAL
{
    HALResource::~HALResource()
    {
    }

    HALTextureView* HALTexture::GetOrCreateTextureView(const HALTextureViewInitializer& view_initializer)
    {
        if (auto iter = views_.find(view_initializer.Hash()); iter != views_.end()) {
            return iter->second;
        }
        auto* view = CreateTextureViewImpl(view_initializer);
        assert(view != nullptr && "create texture view failed");
    }
}