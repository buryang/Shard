#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHIResourceAllocator.h"

namespace MetaInit::RHI {

	class MNIT_API RHIBackEndContext 
	{
	public:
		PooledResourceAllocatorInterface::Ptr GetPoolResourceAllocator();
		TrasiantResourceAllocatorInterface::Ptr GetPoolResourceAllocator();
	private:

	};
}