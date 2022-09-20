#if defined(VUlKAN_BACKEND)

#include "RHI/RHI.h"
#include "RHICommand.h"

namespace MetaInit
{
	namespace RHI
	{
		void RHITexture::SetRHI()
		{
		}

		RHIResource::Ptr RHITexture::GetRHI()
		{
			return RHIResource::Ptr();
		}

		void RHIBuffer::SetRHI()
		{
		}

		RHIResource::Ptr RHIBuffer::GetRHI()
		{
			return RHIResource::Ptr();
		}

		void RHIAccelerate::SetRHI()
		{
		}

		RHIResource::Ptr RHIAccelerate::GetRHI()
		{
			return RHIResource::Ptr();
		}

		//-----------------------------RHI command context logic--------------------------//
		void RHIGraphicsCommandContext::SetRHI()
		{
		}

		bool RHIGraphicsCommandContext::MergeAble(const RHIGraphicsCommandContext& rhs) const
		{
			return false;
		}

		void RHIComputeComandContext::SetRHI()
		{
		}

		bool RHIComputeComandContext::MergeAble(const RHIComputeComandContext& rhs) const
		{
			return false;
		}

	}
}

#endif //VUALKAN_RHI