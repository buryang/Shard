#pragma once

#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"

#include <memory>


namespace MetaInit 
{
	class VulkanDeviceAPI
	{

	};


	class VulkanRendererEngine
	{
	public:
		using Ptr = std::unique_ptr<VulkanRendererEngine>;
		struct VulkanRendererParameters
		{

		};
		using Parameter = VulkanRendererParameters;
		void Init();
		void	 UnInit();
		static Ptr Create(const Parameter&);
		void CreateGraphicsPipeLine();
		~VulkanRendererEngine() { UnInit(); }
	private:
		VulkanRendererEngine() = default;
		VulkanRendererEngine(const VulkanRendererEngine&) = delete;
		VulkanRendererEngine& operator=(const VulkanRendererEngine&) = delete;
		void QueryDevice();
	private:
		VkInstance			instance_{ VK_NULL_HANDLE };
		VkDevice				device_{ VK_NULL_HANDLE };

	};

}

