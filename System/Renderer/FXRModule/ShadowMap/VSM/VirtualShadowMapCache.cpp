#include "VirtualShadowMapCache.h"

namespace Shard::Render {

    //whether vsm cache enabled
    REGIST_PARAM_TYPE(BOOL, VSM_CACHE_ENABLED, true);

    //max light age since last request
    REGIST_PARAM_TYPE(UINT, VSM_MAX_LIGHT_CACHE_AGE_GAP, 1000);

    //instance invalidation whether tested against the HZB
    REGIST_PARAM_TYPE(BOOL, VSM_CACHE_INVALIDATE_HZB, true);
    
    
    VirtualShadowMapLightCache::HashType VirtualShadowMapLightCache::GenerateLightCacheHash(uint32_t lightID, uint32_t viewID, Scene::ELightType light_type)
    {
        const uint32_t data[3] = { lightID, viewID, (uint32_t)light_type };
        return Utils::InternSpookyV2HashForBytes<32>(reinterpret_cast<const char*>(data), sizeof(data));
    }

    VirtualShadowMapLightCache::VirtualShadowMapLightCache(uint32_t lightID, uint32_t view_ID, uint32_t num_cache_entries)
    {
        
    }

    VirtualShadowMapLightCache* VirtualShadowMapCacheManager::FindOrCreateLightCache(VirtualShadowMapLightCache::HashType hash)
    {
        auto iter = light_caches_.find(hash);
        if (iter != light_caches_.end())
        {
            if (iter->cache_entries_.size() == xx)
            {
                return *iter;
            }
            else
            {
                light_caches_.erase(iter);
            }
        }

        auto* light_cache = new 
    }

}
