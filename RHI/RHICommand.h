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
			EFlags	flags_{ EFlags::eUnkown };
			uint32_t	device_mask_{ 0xFFFFFFFF };
		};

		/*a virtual command context, maybe usage: record command ir represent for reuse/debug*/
		class MINIT_API RHIVirtualCommandContext: public RHICommandContext
		{
		public:
			void Enqueue(const RHICommandPacketInterface::Ptr cmd) override;
			//submit command
			void Submit(RHIGlobalEntity::Ptr rhi) override;
			/*re broadcast to other real command context*/
			void ReplayByOther(RHICommandContext::Ptr other_context);
			virtual ~RHIVirtualCommandContext() = default;
		protected:
			Vector<const RHICommandPacketInterface::Ptr> record_cmds_;
		};
	}
}
