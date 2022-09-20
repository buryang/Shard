#pragma once

#include "Renderer/RtRenderResources.h"
#include "Utils/CommonUtils.h"


namespace MetaInit::Renderer
{
	class RtTransiantResourceAllocatorImpl;
	class MINIT_API RtTransiantResourceAllocator
	{
	public:
		void CreateTexture(const RtField& texture);
		void CreateBuffer(const RtField& buffer);
		void ReleaseAllResources();
	private:
		RtTransiantResourceAllocatorImpl*	alloc_impl_;
	};


	class MINIT_API RtPooledResource
	{
	public:
		using SharedPtr = std::shared_ptr<RtPooledResource>;

	};


	//singleton pool resource allocator, lifetime longer than framegraph
	class MINIT_API RtPoolResourceAllocator
	{
	public:
		static RtPoolResourceAllocator& Instance() {
			static RtPoolResourceAllocator allocator;
			return allocator;
		}
		void CreateTexture();
		void CreateBuffer();
		void GarbageCollection();
		size_t	GetMemoryBudget()const { return mem_budget_; }
		void SetMemoryBudget(size_t budget);
	private:
		RtPoolResourceAllocator() = default;
		DISALLOW_COPY_AND_ASSIGN(RtPoolResourceAllocator);
		size_t		mem_budget_;
		Vector<RtPooledResource::SharedPtr>	pool_resources_;
	};
}
