#pragma once
#include "Utils/CommonUtils.h"
#include "VulkanRHI.h"

namespace MetaInit
{
	class VulkanShaderModule;
	VkShaderModuleCreateInfo MakeShaderModuleCreateInfo(const Span<char>& code);
	VkPipelineShaderStageCreateInfo MakeShaderStageCreateInfo(VulkanShaderModule& module, const std::string& name);

	struct VulkanResourceDiscriptors
	{
		std::vector<VkDescriptorSetLayout>	layout_;
		std::vector<int>					dummy_;
	};

	class VulkanShaderModule
	{
	public:
		enum class EType
		{
			VERTEX,
			PIXEL,
			GEOMETRY,
			COMPUTE,
			HULL,
			DOMAINS,
		};
		VulkanShaderModule(VulkanDevice::Ptr device, const std::string& shader_file, EType type);
		DISALLOW_COPY_AND_ASSIGN(VulkanShaderModule);
		~VulkanShaderModule();
		VkShaderModule Get();
		EType Type()const;
	private:
		VkShaderModule			handle_{ VK_NULL_HANDLE };
		VulkanDevice::Ptr		device_;
		EType					shader_type_;
	};
}