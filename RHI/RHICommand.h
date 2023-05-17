#pragma once
#include "Utils/CommonUtils.h"
#include "Renderer/RtRenderBarrier.h"
#include "RHI/RHIGlobalEntity.h"
#include "RHI/RHICommandIR.h"

namespace MetaInit
{
	namespace RHI
	{
		class MINIT_API RHICommandContext
		{
		public:
			enum class EFlags {
				eUnkown,
				eGraphics,
				eCompute,
				eAsyncCompute,
				eTransfer, //fixme whether we need a transfer context
				eNum,
			};
			using Ptr = RHICommandContext*;
			RHICommandContext(EFlags flags) : flags_(flags) {}
			//enqueue command sequeues
			virtual void EnqueuePreEvent(const RHIEvent::Ptr event) = 0;
			virtual void EnqueuePostEvent(const RHIEvent::Ptr event) = 0;
			virtual void Enqueue(const RHICommandPacketInterface::Ptr cmd) = 0;
			//submit command
			virtual void Submit(RHIGlobalEntity::Ptr rhi) = 0;
			virtual ~RHICommandContext() {}
			EFlags GetFlags() const {
				return EFlags::eUnkown;
			}
			void SetFlags(EFlags flags){
				flags_ = flags;
			}
		private:
			DISALLOW_COPY_AND_ASSIGN(RHICommandContext);
		private:
			EFlags flags_{ EFlags::eUnkown };
		};
	}
}
