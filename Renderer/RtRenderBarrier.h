#pragma once
#include "Utils/CommonUtils.h"

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
			void AddTransitionBarrier(RtTransitionBarrier&& transition);
			void AddAliasingBarrier(RtAliasingBarrier&& aliasing);
			void AddUAVBarrier(RtUAVBarrier&& uav);
			void Submit(RHI::RHICommandContext& context);
		private:
			SmallVector<RtTransitionBarrier> transitions_;
			SmallVector<RtAliasingBarrier>	 aliasing_;
			SmallVector<RtUAVBarrier>		uavs_;
		};
	}
}