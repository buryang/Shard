#pragma once
#include "VulkanRHI.h"

namespace MetaInit
{
	namespace Internal {
		class VulkanVMAMemAllocator;
	}
	class VulkanSceneAccStructure
	{
	public:
		VkAccelerationStructureKHR get() {
			return scene_;
		}
		void Build();
		void Update();
	private:
		void BuildTlas();
		void UpdateTlas();
		void BuildBlas();
		void UpdateBlas();
	private:
		VkAccelerationStructureKHR			scene_ = VK_NULL_HANDLE;
		Internal::VulkanVMAMemAllocator		mem_allocator_;
	};


}