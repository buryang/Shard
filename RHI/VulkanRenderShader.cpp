#include "RHI/VulkanRenderShader.h"
#include <fstream>

namespace MetaInit {

	VkShaderModuleCreateInfo MakeShaderModuleCreateInfo(const Span<char>& code)
	{
		VkShaderModuleCreateInfo module_info{};
		memset(&module_info, 0, sizeof(module_info));
		module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		module_info.pCode = reinterpret_cast<const uint32_t*>(code.data());
		//codeSize is the size, in bytes, of the code pointed to by pCode
		module_info.codeSize = code.size();
		return module_info;
	}

	VkPipelineShaderStageCreateInfo MakeShaderStageCreateInfo(VulkanShaderModule& module)
	{
		VkPipelineShaderStageCreateInfo stage_info{};
		memset(&stage_info, 0, sizeof(stage_info));
		stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		using ShaderType = VulkanShaderModule::EType;
		switch (module.Type())
		{
		case ShaderType::eCompute:
			stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			break;
		case ShaderType::eVertex:
			stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
			break;
		case ShaderType::eGeometry:
			stage_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
			break;
		case ShaderType::ePixel:
			stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			break;
		default:
			throw std::runtime_error("not support such shader");
		}
		stage_info.module = module.Get();
		stage_info.pName = module.GetName().c_str();
		return stage_info;
	}

	VulkanShaderModule::VulkanShaderModule(VulkanDevice::Ptr device, EType type, const std::string& shader_file, const std::string& name):device_(device),shader_type_(type), shader_name_(name)
	{
		std::ifstream shader_stream(shader_file, std::ios::binary | std::ios::ate);
		auto stream_size = shader_stream.tellg();
		assert(stream_size % 4 == 0);
		shader_stream.seekg(0, std::ios::beg);
		Vector<char> buffer(stream_size);
		shader_stream.read(buffer.data(), stream_size);
		auto shader_info = MakeShaderModuleCreateInfo(Span<char>(buffer.data(), stream_size));
		auto ret = vkCreateShaderModule(device_->Get(), &shader_info, g_host_alloc, &handle_);
		if (VK_SUCCESS != ret) {
			throw std::runtime_error("load shader failed");
		}
	}

	VulkanShaderModule::~VulkanShaderModule()
	{
		if (VK_NULL_HANDLE != handle_)
		{
			vkDestroyShaderModule(device_->Get(), handle_, g_host_alloc);
		}
	}
	VkShaderModule VulkanShaderModule::Get()
	{
		return handle_;
	}

	VulkanShaderModule::EType VulkanShaderModule::Type() const
	{
		return shader_type_;
	}
}