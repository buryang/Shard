/*****************************************************************//**
 * \file   RenderBarrier.h
 * \brief  https://diligentgraphics.com/2018/12/09/resource-state-management/
 * 
 * \author buryang
 * \date   April 2024
 *********************************************************************/
#pragma once
#include "Utils/CommonUtils.h"
#include "HAL/HALCommand.h"
#include "HAL/HALGlobalEntity.h"
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
            static inline bool IsSupportByQueue(const Transition& transition, EPipeline queue_pipeline) {
                if (queue_pipeline == EPipeline::eAsyncCompute) {
                    if (Utils::HasAnyFlags(transition.prev_access_, EAccessFlags::eIndexBuffer | EAccessFlags::eUAV) ||  //todo check PixelShaderAccess
                        Utils::HasAnyFlags(transition.next_access_, EAccessFlags::eIndexBuffer | EAccessFlags::eUAV)) {
                        return false;
                    }
                    if (Utils::HasAnyFlags(transition.prev_stage_, ~EPipelineStageFlags::eAllCompute) ||
                        Utils::HasAnyFlags(transition.next_stage_, ~EPipelineStageFlags::eAllCompute)) {
                        return false;
                    }
                }
                return true;
            }

        };

        static inline bool IsTextureAccessTransitionImplictAllowed(EAccessFlags prev_access, EAccessFlags next_access) {
            if (GET_PARAM_TYPE_VAL(BOOL, RENDER_BARRIER_IMPLICIT) && GET_PARAM_TYPE_VAL(UINT, HAL_ENTITY_BACKEND) == Utils::EnumToInteger(HAL::EHALBackEnd::eD3D)) {
                //check whether prev access can be implict decay to Common;whether next access can be implictly promotion from common
                if (Utils::HasAnyFlags(prev_access, ~(EAccessFlags::eTransferSrc|EAccessFlags::eTransferDst|EAccessFlags::eSRV|EAccessFlags::eDSV|EAccessFlags::eRTV)) || 
                    Utils::HasAnyFlags(next_access, ~EAccessFlags::eTransferSrc|EAccessFlags::eSRV|EAccessFlags::eDSV|EAccessFlags::eRTV)) {
                    return false;
                }
            }
            return true;
        }

        static inline bool IsBufferAccessTransitionImplicitAllowed() {
            return (GET_PARAM_TYPE_VAL(BOOL, RENDER_BARRIER_IMPLICIT) && GET_PARAM_TYPE_VAL(UINT, HAL_ENTITY_BACKEND) == Utils::EnumToInteger(HAL::EHALBackEnd::eD3D));
        }

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

        enum class EBarrierBatchType
        {
            eUnkown = 0x0,
            eNormal = 0x1,
            eSplitBegin = 0x2,
            eSplitEnd   = 0x4,
        };

        static inline const char* BarrierBatchTypeToChar(EBarrierBatchType type) {
            switch (type)
            {
            case EBarrierBatchType::eNormal:
                return "Normal";
            case EBarrierBatchType::eSplitBegin:
                return "SplitBegin";
            case EBarrierBatchType::eSplitEnd:
                return "SplitEnd";
            default:
                return "Unkown";
            }
        }
        struct BarrierBatch
        {
            FORCE_INLINE void AddTransition(Transition&& transition) {
                transitions_.emplace_back(transition);
            }
            FORCE_INLINE void AddBarrier(Barrier&& barrier) {
                barriers_.emplace_back(barrier);
            }
            FORCE_INLINE bool IsEmpty()const {
                return transitions_.empty() && barriers_.empty();
            }
            void operator+=(BarrierBatch&& other) {
                eastl::move(other.transitions_.begin(), other.transitions_.end(), eastl::back_inserter(transitions_));
                eastl::move(other.barriers_.begin(), other.barriers_.end(), eastl::back_inserter(barriers_));
            }
            EBarrierBatchType   type_{ EBarrierBatchType::eUnkown };
            SmallVector<Transition> transitions_;
            SmallVector<Barrier>    barriers_;
            //prev split barrier sync
        };
    }
}