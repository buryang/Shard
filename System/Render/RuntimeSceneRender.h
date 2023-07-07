#pragma once
#include "Utils/CommonUtils.h"
#include "Scene/Scene.h"

namespace Shard
{
	namespace Runtime
	{
		class RuntimeViewContext
		{
		public:
			using Ptr =	RuntimeViewContext*;
		private:
			vec3	pos_;
			vec3	view_;
		};

		class RuntimeSceneRenderInterface
		{
		public:
			using Ptr = RuntimeSceneRenderInterface*;
			virtual void Render(SceneProxyHelper::SharedPtr scene) = 0;
			virtual ~RuntimeSceneRenderInterface() = default;
		protected:
			SceneProxyHelper::SharedPtr			scene_proxy_;
			//render view context
			Vector<RuntimeViewContext::Ptr>		views_;
		};
	}
}
