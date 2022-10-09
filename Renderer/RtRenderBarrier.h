#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHICommand.h"

namespace MetaInit
{
	namespace Renderer
	{
		//for vulkan barrier need stage information
		enum class PipelineStageFlags
		{
			eUnkown = 0x0,
		};

		struct RtTransitionBarrier
		{
			xxx resource_;
			xxx prev_access_;
			xxx next_access_;
			yyy flags_;
			uint32_t mip_index_;
			uint32_t array_index_;
			uint32_t plane_index_;
			FORCE_INLINE bool operator==(const RtTransitionBarrier& rhs) const {
				return resource_ == rhs.resource_ &&
					prev_access_ == rhs.prev_access_ &&
					next_access_ == rhs.next_access_ &&
					flags_ == rhs.flags_ &&
					mip_index_ == rhs.mip_index_ &&
					array_index_ == rhs.array_index_ &&
					plane_index_ == rhs.plane_index_;
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