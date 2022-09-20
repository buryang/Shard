#pragma once
#include "Utils/CommonUtils.h"
#include "Scene/Scene.h"
#include "RHI/VulkanRHI.h"
#include "RHI/VulkanPrimitive.h"

#include <unordered_map>


namespace MetaInit
{
	class MINIT_API VulkanStaticScene
	{
	public:
		VulkanStaticScene() = default;
		void Load(SceneProxyHelper& scene_helper);
		bool IsIndexed()const;
	private:
		VulkanDevice::Ptr	device_;
		std::unordered_map<uint32_t, Primitive::VulkanImage::Ptr>	materials_;
		Primitive::VulkanBuffer::Ptr	vertex_pos_;
		Primitive::VulkanBuffer::Ptr	vertex_indices_;
	};
}
