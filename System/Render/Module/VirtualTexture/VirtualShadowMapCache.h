#pragma once

#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include "Scene/Primitive.h"
#include "Graphics/Render/RenderGraph.h"
#include "../SceneProxy/SceneProxy.h"

/* I want to realize a system like unreal engine virtual shadow map
 * there's little information about it except source code 
 * read about https://ktstephano.github.io/rendering/stratusgfx/svsm
 * it says that it's like a unreal engine realization
 */

namespace Shard::Render
{

    //property of one cahced shadow map
    class VritualShadowMapCacheEntryProperty
    {

    };

    /**
     * \brief virtual shadow map cache for single light
     */
    class VirtualShadowMapLightCache
    {
    public:
        friend class VirtualShadowMapCacheManager;
        using HashType = Utils::SpookyV2Hash32;
        static HashType GenerateLightCacheHash(uint32_t lightID, uint32_t viewID, Scene::ELightType light_type);
        explicit VirtualShadowMapLightCache(Scene::ELightType light_type, uint32_t num_cache_entries);
    protected:
        //todo
    protected:

        uint32_t last_active_frame_index_{ 0u };
        uint32_t lightID_{ 0u };
        Scene::ELightType light_type_;
        uint32_t is_uncached_ : 1;
        uint32_t is_distant_light_ : 1;
        uint32_t is_use_receiver_mask_ : 1;

        //max physX pages
        uint32_t max_physX_pages_{ 0u };

        HashType hash_;
        //set of shadow map properties, set of either a clipmap(N), cubemap(6) or a regular VSM(1)
        SmallVector<VritualShadowMapCacheEntryProperty> cache_entries_;
    };

    ALIGN_AS(16) struct PageCacheStats
    {
        //todo
        uint32_t curr_cached_pages_;
        uint32_t max_cached_pages_;
        uint32_t curr_invalidate_pages_;
        uint32_t static_cached_pages_;
    };

    enum class EPageOverflowStrategy
    {
        eUnkown,
        eCoarseLevel, //page using coarse level
        eHostReAlloc, //host realloc a new buffer
    };

    class MINIT_API VirtualShadowMapCacheManager : public xx
    {
    public:
        static VirtualShadowMapCacheManager& Instance();
        VirtualShadowMapLightCache* FindOrCreateLightCache(VirtualShadowMapLightCache::HashType hash);
        void PreLightChange(RenderGraph& graph);
        void PrePrimitiveChange(RenderGraph& graph);
        void Tick();
        void PostLightChange(RenderGraph& graph);
        void PostPrimitiveChange(RenderGraph& graph);

        //readback cache manager states
        void ExtractStat(RenderGraph& graph);
    protected:

    private:
        DISALLOW_COPY_AND_ASSIGN(VirtualShadowMapCacheManager);
        Map<VirtualShadowMapLightCache::HashType, VirtualShadowMapLightCache*> light_caches_;
        //actual physical texture data store here rather in virtual shadow map 
        //for persist cached pages between frame
        RenderTexture::Handle physX_pages_pool_;
        RenderTexture::Handle hzb_physX_pages_pool_;
        //todo meta data for pages
        RenderTexture::Handle physX_pages_meta_;
        uint32_t max_physX_pages_{ 0u };

        //persistent primitive index

        //memory budget 
        float global_resolution_lod_bias_{ 0.0f };
        uint32_t prev_frame_over_page_allocation_budget_{ 0u };

        //statistics buffer
        RenderBuffer::Handle stats_buffer_;
    };
}
