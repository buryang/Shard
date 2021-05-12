#pragma once

#define GLFW_INCLUDE_VULKAN
#include "glfw3.h"
#include "vulkan/vulkan.hpp"

#include <memory>


namespace MetaInit 
{
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
		vk::UniqueInstance	instance_{ VK_NULL_HANDLE };
		vk::Device			device_{ VK_NULL_HANDLE };

	};

}

