#include "RHI/Vulkan/RHIResourcesVulkan.h"
#include "RHI/Vulkan/RHIResourceAllocatorVulkan.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"
#include <typeinfo>

namespace MetaInit::RHI::Vulkan
{
	void RHITextureVulkan::operator=(RHITextureVulkan&& rhs)
	{
		texture_ = rhs.texture_;
		rhs.texture_.reset();
		memory_ = std::move(rhs.memory_);
		rhs.memory_ = {};
	}
	/*vulkan resource internal realization not use input external global entity handle*/
	void RHITextureVulkan::SetUp()
	{
		assert(typeid(parent_)==typeid(RHIGlobalEntityVulkan));
		//simply check resource already setup
		if (memory_.IsValid()) {
			return;
		}
		dynamic_cast<RHIGlobalEntityVulkan::Ptr>(parent_)->SetUpTexture(this);
	}
	void RHITextureVulkan::Release()
	{
		ReleaseMemoryAllocation(memory_);
	}
	size_t RHITextureVulkan::GetOccupySize() const
	{
		return size_t();
	}
	void RHIBufferVulkan::operator=(RHIBufferVulkan&& rhs)
	{
		buffer_ = rhs.buffer_;
		rhs.buffer_.reset();
		memory_ = std::move(rhs.memory_);
		rhs.memory_ = {};
	}
	void RHIBufferVulkan::SetUp()
	{
		assert(typeid(parent_) == typeid(RHIGlobalEntityVulkan));
		//simply check resource already setup
		if (memory_.IsValid()) {
			return;
		}
		dynamic_cast<RHIGlobalEntityVulkan::Ptr>(parent_)->SetUpBuffer(this);
	}
	void RHIBufferVulkan::Release()
	{
		ReleaseMemoryAllocation(memory_);
	}
	void* RHIBufferVulkan::MapBackMem()
	{
		return buffer_->Map();
	}
	void RHIBufferVulkan::UnMapBackMem()
	{
		buffer_->Unmap();
	}
}