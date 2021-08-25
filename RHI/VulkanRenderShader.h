#pragma once
#include "Utils/CommonUtils.h"
#include "VulkanRHI.h"

namespace MetaInit
{
	class VulkanShaderModule;
	VkShaderModuleCreateInfo MakeShaderModuleCreateInfo(const Span<char>& code);
	VkPipelineShaderStageCreateInfo MakeShaderStageCreateInfo(VulkanShaderModule& module);

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
			eVertex,
			ePixel,
			eGeometry,
			eCompute,
			eHull, //Tessellation
			eDomain,
			//ray tracing part
			eRaygen,
			eAnyHit,
			eMiss,
			eIntersction,
			eMesh, //Mesh shader 
			eCallable,
		};
		using Ptr = std::shared_ptr<VulkanShaderModule>;
		VulkanShaderModule(VulkanDevice::Ptr device, EType type, const std::string& shader_file, const std::string& name);
		DISALLOW_COPY_AND_ASSIGN(VulkanShaderModule);
		~VulkanShaderModule();
		VkShaderModule Get();
		EType Type()const;
		const std::string& GetName()const;
	private:
		VkShaderModule			handle_{ VK_NULL_HANDLE };
		VulkanDevice::Ptr		device_;
		EType					shader_type_;
		std::string				shader_name_;
	};
}