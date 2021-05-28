#pragma once
#include "Utils/CommonUtils.h"
#include "VulkanRHI.h"

namespace MetaInit
{
	struct VulkanResourceDiscriptors
	{
		std::vector<VkDescriptorSetLayout>	layout_;
		std::vector<int>						dummy_;
	};

	template<class Shader>
	class VulkanShaderInstance
	{

	};
}