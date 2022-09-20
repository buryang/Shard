#include "RHI/VulkanStaticScene.h"

namespace MetaInit
{
	void VulkanStaticScene::Load(SceneProxyHelper& scene_helper)
	{

	}

	bool VulkanStaticScene::IsIndexed() const
	{
		return vertex_indices_;
	}
}