#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/SimpleCoro.h"
#include "../../SceneProxy/SceneProxy.h"
#include "../Base/FXRBaseModule.h"

namespace Shard::Renderer::FXR
{
	struct ECSShadowRenderTargetAtlas
	{
		SmallVector<Render::TextureHandle> depth_atlas_;
		Render::TextureHandle depth_stencil_;
	};

	struct ShadowRender
	{
		//float4x4 view_proj_;
		//float   near_plane_;
		//float   far_plane_;
        Utils::Entity light_id_;
        float   bias_;
		float   normal_bias_;
		float   texel_size_; //one texel size in world space
		float   filter_size_; //filter size in world space
		uint32_t	flags_;

		uint4 scissor_rect_; //sub shadow map size and region

#ifndef SM_GPU_DRIVEN_RENEDER
		//static mesh instances batches need rendering
		Vector<> static_mesh_batches_;
		//static mesh draw command ir represents
		Vector<> static_mesh_draw_commands_;
		//dynamic mesh instances batches need rendering

#endif //SM_GPU_DRIVEN_RENEDER 
	};


	/** fundamental elements for a shadow render pass*/
	struct ShadowDrawPassData
	{
		//todo
		SmallVector<ShadowRender> shadows_;
	};

	/*cpu warper for shadow render, split functions out of render
	 * for lately realize some logic on GPU device
	 */
	class ShadowRenderHelper
	{
	public:
		explicit ShadowRenderHelper(ShadowRender* render) :render_(render) {}
		//todo define light info for shadow and shading
		void InitPointLightShadowRender(xx light_info, uint2 resolution, uint border_size);
		void InitDirectionalLightShadowRender(xx light_info);
		void InitSpotLightShadowRender(xx light_info);
		void InitTranslucentShadowRender(xx light_info);

        //property functions
        uint32_t GetShadowFlags() const { return render_->flags_; }
        uint32_t GetShadowViews() const { return check_point_light ? 6u : 1u };
#ifndef SM_GPU_DRIVEN_RENEDER
		//collect render related mesh instances
		void GatherRenderMeshElements();
		void RenderDepth(Render::RenderGraphBuilder& builder);
#endif //SM_GPU_DRIVEN_RENEDER
		~ShadowRenderHelper() = default;
	protected:
        //calculate scissor rect for shadow render
        void CalcRenderScissorRect(uint4& scissor_rect); 
	protected:
		ShadowRender* render_{ nullptr };
	};


	class FXRShadowMapDraw final: public FXRDrawBase
	{
	public:
        DECLARE_RENDER_INDENTITY(FXRShadowMapDraw);
        struct ViewRenderState
        {
            //shadow map render state for each view
         
        };
		void Init(Render::SceneProxy* scene_proxy); //initial all shadow map should be drawn
		void UnInit();
		void Draw(Render::RenderGraphBuilder& builder, Span<ViewRender>& view_renders, ERenderPhase phase) override;
		/* traversal all lights and collect information of lights which need rendering VSM
		 * return false when no light need VSM rendering
		 */
		bool GatherShadowPrimitives();
        //begin async gather shadows to be rendered
		bool BeginAsyncGatherShadowPrimitives(); //async gather shadow render data
		bool FinalizeAsyncGatherShaowPrimitives();
        bool GatherShadowMeshElements();
        //begin async gather mesh element need to be rendered for shadow maps
        void BeginAsyncGatherShadowMeshElements();
        void FinalizeAsyncGatherShadowMeshElements();
        ~FXRShadowMapDraw();
	private:
		/** generate mip views from primary views */
		void GenerateMipViews(const Span<ShadowView>& primary_views,);
		/*distribute */
		void DistributePackedViewToCullingPasses(bool use_single_pass);
		/*allocate shadow map resource*/
		void AllocateShadowMapRenderTargets();

		void AllocateDirectionalShadowMapRenderTargets();
		void AllocatePointShadowMapRenderTargets();
		//todo vsm shadow map initializer

        //the main render function entrance
        void RenderShadowMap(Render::RenderGraphBuilder& builder, Span<ViewRender>& view_renders);
	private:
		Renderer::SceneProxy* scene_{ nullptr };
		SmallVector<ShadowDrawPassData> shadow_passes_; /*culling pass data*/

		//shadow map render targets for all shadow types
		SmallVector<ShadowRender>	directional_shadow_atlas_;
		SmallVector<ShadowRender>	point_shadow_cubemap_;
		SmallVector<ShadowRender>	spot_shadow_atlas_;

		//coro job enties
        JobHandle   create_shadow_renders_job_;
		JobHandle   gather_primitve_job_;
		JobHandle   filter_shadow_renders_job_;

        //which stage to render, prerender/render
        uint32_t    is_render_early_enabled_ : 1;

        //render state for each view
        DECLARE_RENDER_STATE_OP();
	};
}
