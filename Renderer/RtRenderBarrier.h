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

		};

		struct RtAliasingBarrier
		{

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