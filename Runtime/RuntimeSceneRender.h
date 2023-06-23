#pragma once
#include "Utils/CommonUtils.h"
#include "Scene/Scene.h"

namespace Shard
{
	namespace Runtime
	{
		class MINIT_API RuntimeViewContext
		{
		public:
			using Ptr = std::shared_ptr<RuntimeViewContext>;
		private:

		};

		class MINIT_API RuntimeSceneRenderInterface
		{
		public:
			virtual void Render(SceneProxyHelper::SharedPtr scene) = 0;
		protected:
			SceneProxyHelper::SharedPtr			scene_proxy_;
			//render view context
			Vector<RuntimeViewContext::Ptr>		views_;
		};
	}
}
