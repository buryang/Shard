#pragma once
#include "RHI/RHIResource.h"

namespace MetaInit::RHI
{
	//pooled texture use
	class PooledResourceAllocatorInterface
	{
	public:
		using Ptr = PooledResourceAllocatorInterface*;
		template<class ResourceRHI, class ResrouceDesc>
		ResourceRHI::Ptr AllocResource(const ResourceDesc& desc);
		virtual ~PooledResourceAllocatorInterface() {}
	private:
		RHIResource::Ptr FindSuitAbleFreeResource();
	private:
		struct ResourceData
		{

		};
		Vector<ResourceData>	FreeResources_;
	};

	class TransiantResourceHeapBulk;
	class TrasiantResourceAllocatorInterface
	{
	public:
		using Ptr = TrasiantResourceAllocatorInterface*;
		virtual ~TrasiantResourceAllocatorInterface() {}
	private:
		List<TransiantResourceHeapBulk>	resource_blks_;
	};
}
