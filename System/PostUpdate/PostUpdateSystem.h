#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/SimpleEntitySystem.h"
#include "Utils/SimpleEntitySystemPrimitive.h"
#include "Utils/SimpleCoro.h"
#include "Utils/Memory.h"
#include "Scene/Primitive.h"

namespace Shard::PostUpdate
{

    using VisibilityQuery = ECSComponentQuery<Utils::ECSOwnedList<ECSRenderAbleTag, ECSSpatialBoundBoxComponent>, Utils::ECSExcludedList<> >; //todo

    //view query
    using ViewQuery = ECSComponentQuery<Utils::ECSOwnedList<ECSViewComponent>, Utils::ECSExcludedList<> >;

    //visible entity list for each view entity
    using ECSVisibilityEntities = List<Entity, >; //todo the list allocator

    //bitmask for entity all-view visible
    using ECSVisibilityBitmask = uint32_t;

    struct LodRange
    {
        uint16_t lod0_ : 7;
        uint16_t lod1_ : 7;
        //if is_range is true, range[lod0, lod1], else a lod index as lod0
        uint16_t is_range_ : 1;
        uint16_t is_valid_ : 1;
    };

    //lod index data
    struct ECSLodComponent
    {
        int8_t curr_lod_index_;
        int8_t min_lod_index_;
        float screen_size_[MAX_STATIC_MESH_LODS];
    };

    //view fade start/end range
    struct ECSViewFadeRange
    {
        float2 start_margin_;
        float2 end_margin_;
    };

    //map from mesh id to boolean fade flag
    using ECSViewFadeInMap = BitVector<xx>;
    using ECSViewFadeOutMap = BitVector<xx>;

    struct ECSVisibilityHistory
    {
        //packed 2 bit visibility state, bit-0 for current frame while bit-1 for last frame
        //look for entities which were visible prev-frame but are not visibile curr-frame, we set to hidden
        uint8_t visibility_{ 0u }; 
    };

    using ECSNoFrsutumCullingTag = ECSTagComponent<struct NoFrumstumCulling_>;

    //check whether a lod in lod_range
    inline void IsRangeContainsLod(const LodRange& range, uint16_t lod_index)
    {
        if (range.is_valid_ && range.is_range_) {
            return lod_index < range.lod1_ && lod_index > range.lod0_;
        }
        return false;
    }

    //check instance visible in view
    inline bool IsAllVisibileInView(const ECSViewFadeRange& view_range, float view_distance)
    {
        return view_distance > view_range.start_margin_.x && view_distance < view_range.end_margin_.y;
    }

    inline bool IsCurrFrameVisible(const ECSVisibilityHistory& vb) {
        return vb.visibility_ & 0x1;
    }

    inline bool IsPrevFrameVisible(const ECSVisibilityHistory& vb) {
        return vb.visibility_ & 0x2;
    }

    inline bool IsVisibilityHiddenToggle(const ECSVisibilityHistory& vb) {
        return (vb.visibility_ & 0x3) == 0x2; //bits:10
    }

    inline bool FlipFrameVisibile(ECSVisibilityHistory& vb) {
        vb.visibility_ <<= 1u;
    }

    inline bool IsEntityVisibleInView(const ECSVisibilityBitmask& bitmask, uint32_t view_index)
    {
        return (bitmask & (1u << view_index)) != 0u;
    }

    inline bool IsEntityVisibile(const ECSVisibilityBitmask& bitmask)
    {
        return bitmask != 0u;
    }

    /**
     * \brief system run after physics simulation, before extract system
     */
    class PostUpdateSystem : public Utils::ECSSystem
    {
    public:
        void Init() override;
        void UnInit() override;
        void Update(Utils::ECSSystemUpdateContext& ctx) override;
        bool IsPhaseUpdateBefore(const ECSSystem& other, Utils::EUpdatePhase phase)const override;
        bool IsPhaseUpdateEnabled(Utils::EUpdatePhase phase)const override;
    protected:
        Coro<void> UpdateLightInfo(xxx);
        Coro<void> UpdateViewVisibility(const ViewInstance& view_instance);
    };
}
