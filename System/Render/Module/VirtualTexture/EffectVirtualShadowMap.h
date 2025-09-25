#pragma once
#include "../../SceneProxy/SceneProxy.h"

namespace Shard::Effect
{
	struct ShadowProjectionInfo
	{
		mat4    view_proj_;
		float   near_plane_;
		float   far_plane_;
		float   bias_;
		float   normal_bias_;
		float   texel_size_; //one texel size in world space
		float   filter_size_; //filter size in world space
		uint32_t    shadow_map_size_; //shadow map resolution
		uint32_t	flags_;
	};

	class EffectVirtualShadowMapDraw : public EffectDrawBase
	{
	public:
		void Init(Render::SceneProxy* scene_proxy);
		void UnInit();
		void PreDraw(Render::RenderGraphBuilder& builder) override;
		void Draw(Render::RenderGraphBuilder& builder) override;
		void PostDraw(Render::RenderGraphBuilder& builder) override;
		/* traversal all lights and collect information of lights which need rendering VSM
		 * return false when no light need VSM rendering
		 */
		bool GatherLightsVSMEnabled();
	private:
		Renderer::SceneProxy* scene_{ nullptr };
		SmallVector<ShadowProjectionInfo>	shadow_projections_;

	};
}
