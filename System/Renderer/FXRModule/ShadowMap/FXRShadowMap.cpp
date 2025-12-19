#include "FXRShadowMap.h"

namespace Shard::Renderer::FXR
{
	REGIST_PARAM_TYPE(UINT, SM_MEM_MAX_MB_SIZE, 256u);
	REGIST_PARAM_TYPE(UINT, SM_MAX_RESOLUTION_SIZE, 4096u);
	REGIST_PARAM_TYPE(UINT, SM_MAX_RESOLUTION_SIZE_X, 4096u);
	REGIST_PARAM_TYPE(UINT, SM_MAX_RESOLUTION_SIZE_Y, 4096u);
	REGIST_PARAM_TYPE(FLOAT, SM_LOD_DISTANCE_FACTOR£¬ 1.f); //factor for calculating shadow LOD distance

	REGIST_PARAM_TYPE(BOOL, SM_MULTI_THREAD_NUM, 1u); //single thread as default
	REGIST_PARAM_TYPE(UINT, SM_MESH_GATHER_JOBS, 4u); // number of jobs for gathering mesh instances

	REGIST_PARAM_TYPE(UINT, SM_FADE_RESOLUTION, 512u); //resolution for fading shadows

    REGIST_PARAM_TYPE(BOOL, SM_RENDER_EARLY, false); //if forward rendering/or set to true, render shadow map early

	REGIST_PARAM_TYPE(BOOL, VSM_CULLING_MULTIPASSES_ENABLED, false);
	REGIST_PARAM_TYPE(UINT, VSM_CULLING_PASS_MAX_VIEWS, 4u);

	//define shadow render jobs affinity
	static constexpr uint32_t SHADOW_RENDER_JOB_AFFINITY = 2u; //render job affinity
	

	void ShadowRenderHelper::InitPointLightShadowRender(xx light_info, uint2 resolution, uint border_size)
	{
		//todo
	}

	void ShadowRenderHelper::InitDirectionalLightShadowRender()
	{
		//todo
	}

	bool FXRVirtualShadowMapDraw::GatherVSMShadowPrimitives()
	{
	}

	void FXRVirtualShadowMapDraw::DistributePackedViewToCullingPasses(bool use_single_pass)
	{
		const auto pass_max_views = GET_PARAM_TYPE_VAL(UINT, VSM_CULLING_PASS_MAX_VIEWS);

		//clear culling passes
		culling_passes_.clear();

		VSMDrawPassData pass_data;
		for (const auto& proj : shadow_projections_)
		{

			if (!use_single_pass && pass_data.shadows_.size() + proj.xx >= pass_max_views) //check number of rendered views
			{
				culling_passes_.emplace_back(pass_data);
				pass_data = VSMDrawPassData(); //clear
			}

			if (IsVSMNeeded(proj))
			{
				//append shadow view to curr pass
				pass_data.shadows_.emplace_back(proj);
			}

		}

		culling_passes_.emplace_back(pass_data);
	}

    void FXRShadowMapDraw::Init(Render::SceneProxy* scene_proxy)
    {
        bool is_shadow_render_early = true; //todo
        shadow_render_phase_ = is_shadow_render_early ? ERenderPhase::ePreRender : ERenderPhase::eRender;
    }

    bool FXRShadowMapDraw::GatherShadowPrimitives()
    {
        //warp async gather shadow processing
        BeginAsyncGatherShadowPrimitives();
        return FinalizeAsyncGatherShaowPrimitives();
    }

    bool FXRShadowMapDraw::BeginAsyncGatherShadowPrimitives()
    {
        
        auto TraversalLights = [&, this]()->Coro<> {
            co_return;
        };

        auto InitShadowRender = [&, this]()->Coro<> {
            co_return;
        };

        Utils::Schedule([&, this]() {
            //traversal all visible lights 

            //init shadow render for all lights need shadow map
            co_await Utils::AwaitTupleHelper::Parallel(InitShadowRender()); //todo
        }, nullptr, SHADOW_RENDER_JOB_AFFINITY);
        return true;
    }

    bool FXRShadowMapDraw::FinalizeAsyncGatherShaowPrimitives()
    {
        if (nullptr != create_shadow_renders_job_)
        {
            //wait job finish
        }

        //fillter shadow 
        return true;
    }

    void FXRShadowMapDraw::BeginAsyncGatherShadowMeshElements()
    {
        SmallVector<ShadowRender> shadows_to_gather;

        for ()
        {

        }

        const auto shadow_map_count = shadows_to_gather.size();
        const auto mesh_gather_jobs = GET_PARAM_TYPE_VAL(UINT, SM_MESH_GATHER_JOBS);

        if (shadows_to_gather.empty())
        {
            return;
        }

        //gather mesh elements for each shadow render in parallel
        Utils::Dispatch([&, this](uint32_t sm_index, uint32_t thread_index) {
                auto& shadow_render = shadows_to_gather[sm_index];
                ShadowRenderHelper shadow_render_helper(&shadow_render);
                if (shadow_render_helper.GetShadowViews() > 1u)
                {
                    //todo 
                    //add instance culling work...
                }
            }, 1u, mesh_gather_jobs);

    }
}
