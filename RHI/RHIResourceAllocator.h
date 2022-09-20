#pragma once
#include "RHI/RHIResource.h"

namespace MetaInit::RHI
{
	class PooledResourceAllocatorInterface
	{
	public:
		using Ptr = PooledResourceAllocatorInterface*;
		virtual ~PooledResourceAllocatorInterface() {}
	private:
		RHIResource::Ptr FindSuitAbleFreeResource();
	private:
		struct ResourceData
		{

		};
		Vector<ResourceData>	FreeResources_;
	};

	class TrasiantResourceAllocatorInterface
	{
	public:
		using Ptr = TrasiantResourceAllocatorInterface*;
		virtual ~TrasiantResourceAllocatorInterface() {}
	private:

	};
}
