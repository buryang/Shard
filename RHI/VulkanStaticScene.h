#pragma once
#include "Utils/CommonUtils.h"
#include "Scene/Scene.h"

namespace MetaInit
{
	class MINIT_API VulkanStaticScene
	{
	public:
		VulkanStaticScene() = default;
		void Load(SceneProxyHelper& scene_helper);
	private:
		
	};
}
