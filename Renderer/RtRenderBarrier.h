#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHICommand.h"
#include "Renderer/RtRenderResources.h"

namespace MetaInit
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
			RtRenderResource::Ptr	resource_;
			RtField::EAccessFlags	prev_access_;
			RtField::EAccessFlags	next_access_;
			EPipelineStageFlags	prev_stage_;
			EPipelineStageFlags	next_stage_;
			union
			{
				TextureSubRange		texture_sub_range_;
				BufferSubRange		buffer_sub_range_;
			};
			FORCE_INLINE bool operator==(const RtTransitionBarrier& rhs) const {
				return resource_ == rhs.resource_ &&
					prev_access_ == rhs.prev_access_ &&
					next_access_ == rhs.next_access_ &&
					prev_stage_ == rhs.prev_stage_ &&
					next_stage_ == rhs.next_stage_ &&
					sub_range_ = rhs.sub_range_;
			}
			FORCE_INLINE bool operator!=(const RtTransitionBarrier& rhs) const {
				return !(*this == rhs);
			}

		};

		struct RtAliasingBarrier
		{

			yyy flags_;
		};

		struct RtUAVBarrier
		{
			//vulkan just clear global cache
			RtRenderResource::Ptr	resource_;
			TextureSubRangeIndex	sub_range_;
		};

		class RtBarrierBatch
		{
		public:
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
				dependencies_ = deps;
			}
			void Submit(RHI::RHICommandContext& context);
		private:
			SmallVector<RtTransitionBarrier>	transitions_;
			SmallVector<RtAliasingBarrier>		aliasing_;
			SmallVector<RtUAVBarrier>			uavs_;
			//prev split barrier sync
			RtBarrierBatch::Ptr					dependencies_{ nullptr };
		};
	}
}