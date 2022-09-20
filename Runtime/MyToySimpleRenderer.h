#pragma once
#include <string>
#include "Runtime/RuntimeSceneRender.h"
#include "Utils/CommonUtils.h"
#include "Renderer/RtRenderGraphBuilder.h"

namespace MetaInit
{
	namespace Runtime
	{
		class MINIT_API MyToySimpleRenderer final : public RuntimeSceneRenderInterface
		{
		public:
			//fixme register parameter anywhere like ue
			struct RendererCfg
			{
#ifdef RAY_TRACING
				struct RayTracingCfg
				{
					bool	use_rt_AO_{	false };
					bool	use_rt_GI_{ false };
					bool	use_rt_Occlusion_{ false };

					//cpu culling config
					float	cull_maximum_minradius_;
					float	cull_frustum_angle_;
				};
#endif
				struct RasterCfg
				{
					//cpu culling now
					bool	use_cpu_culling_{ true }; 
				};

				//render graph parameters
				bool						use_immediate_mode_{ false };
				bool						use_async_compute_{ false };
				bool						use_custom_msaa_{ false };

				//read effect
				bool						with_velocity_rendered_{ false };
				bool						with_volumetric_cloud_rendered_{ false };
				bool						with_volumetric_fog_rendered_{ false };
				bool						with_sky_atmosphere_rendered_{ false };

				void Load(const std::string& path);
				void Dump(const std::string& path);
			};
			MyToySimpleRenderer(RendererCfg& cfg);
			void Render(SceneProxyHelper::SharedPtr scene) override;
			const RendererCfg& Config()const;
		private:
			RendererCfg						config_;
		private:
			bool GatherDyamicMeshDrawCommand(Renderer::RtRenderGraphBuilder& builder);
			bool GatherRayTracingWorld(Renderer::RtRenderGraphBuilder& builder);
		};
	}
}
