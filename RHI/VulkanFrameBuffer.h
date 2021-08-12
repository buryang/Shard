#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/VulkanPrimitive.h"

namespace MetaInit
{
	class VulkanFrameBuffer
	{
	public:
		struct Attachment
		{
			Primitive::VulkanImageView	attach_;
			uint32_t					mip_level_ = 0;
			uint32_t					array_size_ = 1;
			uint32_t					first_array_slice_ = 0;
		};
		VulkanFrameBuffer(Span<Attachment>& attchments);
		~VulkanFrameBuffer();
		VkFramebuffer Get() { return handle_; }
	private:
		VkFramebuffer			handle_{ VK_NULL_HANDLE };
		Vector<Attachment>		attachments_;
	};
}