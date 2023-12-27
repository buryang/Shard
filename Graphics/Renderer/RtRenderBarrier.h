#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHICommand.h"
#include "Renderer/RtRenderResources.h"

namespace Shard
{
     namespace Renderer
     {
          //for vulkan barrier need stage information
          enum class EPipelineStageFlags
          {
               eUnkown = 0,
               eTopOfPipe = 1 << 0,
               eDrawIndirect = 1 << 1,
               eVertexInput = 1 << 2,
               eShaderVertex = 1 << 3,
               eShaderTControl = 1 << 4,
               eShaderTEval = 1 << 5,
               eShaderGeometry = 1 << 6,
               eShaderFrag = 1 << 7,
               eShaderCompute = 1 << 8,
               eAllGFX = 1 << 9,
               eAllCommand = 1 << 10,
               eBuildAS = 1 << 11,
               eRayTrace = 1 << 12,
               eBottomOfPipe = 1 << 13,
          };

          struct RtTransitionBarrier
          {
               RtRenderResource::Ptr     resource_;
               EAccessFlags     prev_access_;
               EAccessFlags     next_access_;
               EPipelineStageFlags     prev_stage_{ EPipelineStageFlags::eUnkown };
               EPipelineStageFlags     next_stage_{ EPipelineStageFlags::eUnkown };
               union
               {
                    TextureSubRange          texture_sub_range_;
                    BufferSubRange          buffer_sub_range_;
               };
               FORCE_INLINE bool operator==(const RtTransitionBarrier& rhs) const {
                    return std::memcmp(this, &rhs, sizeof(*this))==0;
               }
               FORCE_INLINE bool operator!=(const RtTransitionBarrier& rhs) const {
                    return !(*this == rhs);
               }
          };

          struct RtAliasingBarrier
          {
               RtRenderResource::Ptr     resource_;
               //yyy flags_;
          };

          struct RtUAVBarrier
          {
               //vulkan just clear global cache
               RtRenderResource::Ptr     resource_;
               TextureSubRange     sub_range_;
          };

          struct RtBarrierBatch
          {
               using Ptr = RtBarrierBatch*;
               FORCE_INLINE void AddTransitionBarrier(RtTransitionBarrier&& transition) {
                    transitions_.emplace_back(transition);
               }
               FORCE_INLINE void AddAliasingBarrier(RtAliasingBarrier&& aliasing) {
                    aliasing_.emplace_back(aliasing);
               }
               FORCE_INLINE void AddUAVBarrier(RtUAVBarrier&& uav) {
                    uavs_.emplace_back(uav);
               }
               FORCE_INLINE void AddDependencies(RtBarrierBatch::Ptr deps) {
                    dependencies_.insert(deps);
               }
               SmallVector<RtTransitionBarrier>     transitions_;
               SmallVector<RtAliasingBarrier>          aliasing_;
               SmallVector<RtUAVBarrier>               uavs_;
               //prev split barrier sync
               Set<RtBarrierBatch::Ptr>               dependencies_{ nullptr };
          };
     }
}