#include "VulkanRHI.h"
#include "GLM/glm.hpp"

#include <string>
#include <vector>
#include <fstream>

namespace 
{
	const char* 
}

namespace MetaInit 
{
	void VulkanRendererEngine::Init()
	{

	}

	void	 VulkanRendererEngine::UnInit()
	{

	}

	Ptr VulkanRendererEngine::Create(const Parameter& params)
	{

	}

	void VulkanRendererEngine::CreateGraphicsPipeLine()
	{
		/*spv load function*/
		auto load_spv = [&](const std::string& file_name){
			std::ifstream spv_file(file_name, std::ios::ate|std::ios::binary);
			auto file_size = spv_file.tellg();
			spv_file.seekg(0);
			std::vector<uint8_t> spv_bytes(file_size);
			spv_file.read(spv_bytes.data(), file_size);
			return spv_bytes;
		};
	}
}