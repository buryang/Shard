#include "VulkanFrameBuffer.h"

MetaInit::VulkanFrameBuffer::VulkanFrameBuffer(Span<Attachment>& attchments)
{
}

MetaInit::VulkanFrameBuffer::~VulkanFrameBuffer()
{
	if (VK_NULL_HANDLE != handle_)
	{
		vkDestroyFramebuffer(nullptr, handle_, g_host_alloc);
	}
}
