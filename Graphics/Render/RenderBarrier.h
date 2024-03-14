#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHICommand.h"
#include "Render/RenderResources.h"

namespace Shard
{
    namespace Render
    {
        struct Transition
        {
            RenderResource* resource_;
            EAccessFlags    prev_access_;
            EAccessFlags    next_access_;
            EPipelineStageFlags prev_stage_{ EPipelineStageFlags::eAll };
            EPipelineStageFlags next_stage_{ EPipelineStageFlags::eAll };
            union
            {
                TextureSubRangeRegion   texture_sub_range_;
                BufferSubRangeRegion    buffer_sub_range_;
            };
            FORCE_INLINE bool operator==(const Transition& rhs) const {
                return std::memcmp(this, &rhs, sizeof(*this))==0;
            }
            FORCE_INLINE bool operator!=(const Transition& rhs) const {
                return !(*this == rhs);
            }
            static inline bool IsGlobalBarrier(const Transition& transition) {
                return transition.resource_ == nullptr;
            }
        };

        enum class EBarrierType
        {
            eInvalidate,//d3d12 alising barrier
            eFlush, //d3d12 uav barrier
        };

        struct Barrier
        {
            EBarrierType    type_;
            RenderResource* resource_{ nullptr };
            EPipelineStageFlags prev_stage_{ EPipelineStageFlags::eAll };
            EPipelineStageFlags next_stage_{ EPipelineStageFlags::eAll };

            FORCE_INLINE bool operator==(const Barrier& rhs) const {
                return std::memcmp(this, &rhs, sizeof(*this)) == 0;
            }
            FORCE_INLINE bool operator!=(const Barrier& rhs) const {
                return !(*this == rhs);
            }
        };

        struct BarrierBatch
        {
            using Ptr = BarrierBatch*;
            FORCE_INLINE void AddTransition(Transition&& transition) {
                transitions_.emplace_back(transition);
            }
            FORCE_INLINE void AddBarrier(Barrier&& barrier) {
                barriers_.emplace_back(barrier);
            }
            FORCE_INLINE void AddDependencies(BarrierBatch::Ptr deps) {
                dependencies_.insert(deps);
            }
            FORCE_INLINE bool IsEmpty()const {
                return transitions_.empty() && barriers_.empty() && dependencies_.size();
            }
            void operator+=(BarrierBatch&& other) {
                eastl::move(other.transitions_.begin(), other.transitions_.end(), eastl::back_inserter(transitions_));
                eastl::move(other.barriers_.begin(), other.barriers_.end(), eastl::back_inserter(barriers_));
            }
            SmallVector<Transition> transitions_;
            SmallVector<Barrier>    barriers_;
            //prev split barrier sync
            Set<BarrierBatch::Ptr>  dependencies_{ nullptr };
        };
    }
}