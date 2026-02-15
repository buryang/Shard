#include "Core/EngineGlobalParams.h"
#include "Scene/Primitive.h"
#include "PostUpdateSystem.h"

namespace Shard::PostUpdate
{
    //whether enable view across-fade effect, or just abrupt show/hide
    //when enabled a dithered transition pass will enabled
    REGIST_PARAM_TYPE(BOOL, VIEW_FADING_ENABLED, false);
    //range of distances in world units where the fading starts and ends
    REGIST_PARAM_TYPE(FLOAT, VIEW_FADE_OUT_START, 800u);
    REGIST_PARAM_TYPE(FLOAT, VIEW_FADE_OUT_END, 1000u);
    REGIST_PARAM_TYPE(FLOAT, VIEW_FADE_IN_START, 0u);
    REGIST_PARAM_TYPE(FLOAT, VIEW_FADE_IN_END, 100u);
    //if set to true, will use aabb' center to do fade distance calculation, otherwise use the origin of the mesh
    REGIST_PARAM_TYPE(BOOL, VIEW_FADE_USING_AABB, true);

    //entity frustum check
    REGIST_PARAM_TYPE(BOOL, VIEW_FRUSTUM_SPHERE, true);

    //LOD distance factor
    //force mesh lod, default ~0u mean not force
    REGIST_PARAM_TYPE(UINT, VIEW_LOD_FORCE_VAL, ~0u);
    //engine global lod factor bias
    REGIST_PARAM_TYPE(FLOAT, VIEW_LOD_FACTOR_BIAS, 1.f);


    //minest culling screen size
    REGIST_PARAM_TYPE(FLOAT, VIEW_MIN_SCREEN_SIZE, 0.01f);

    //calculate aabb's screen size in view space
    float CalcBoundsScreenSize(const float3& box_center, const float sphere_radius, const float3& view_origin, const float4x4 view_proj)
    {
        const float dist = glm::length(box_center - view_origin);

        //get projection multiple accounting for view scaling
        const float screen_multiple = eastl::max(view_proj[0][0], view_proj[1][1]) * 0.5f;

        //calculate screen-space projected radius
        const float screen_radius = (sphere_radius / eastl::max(1.f, dist)) * screen_multiple;
        return screen_radius * 2.f; //diameter
    }

    float CalcBoundsScreenRadiusSquared(const float3& box_center, const float sphere_radius, const float3& view_origin, const float4x4 view_proj)
    {
        const float dist_sqr = glm::length2(box_center - view_origin);
        //get projection multiple accounting for view scaling
        const float screen_multiple = eastl::max(view_proj[0][0], view_proj[1][1]) * 0.5f;
        return (sphere_radius * screen_multiple) * (sphere_radius * screen_multiple) / eastl::max(1.f, dist_sqr);
    }

    //reverse calculate distance from screen size
    float CalcBoundsDistanceFromScreenSize(const float sphere_radius, const float screen_size, const float4x4 view_proj)
    {
        const float screen_multiple = eastl::max(view_proj[0][0], view_proj[1][1]) * 0.5f;
        return (sphere_radius * screen_multiple) / eastl::max(FLT_MIN, screen_size * 0.5f); //diameter*0.5 to radius
    }

    bool CheckInstanceViewRelated(const xx& view, const ECSInstanceLayerComponent& instance_layer) {
        if (/*shadow view*/ && !instance_layer.is_shadow_caster_) {
            return false;
        }
        //todo
        return true;
    }

    /*LOD related function£¬ unreal engine used complex lod logic, which test platform feature/mesh defination/material support etc FIXME*/
    int8_t CalcTemporalStaticInstanceLOD(const ECSLodComponent& lod_data, const float3& box_center, const float radius, int32_t min_lod)
    {
        const auto screen_size_sqr = CalcBoundsScreenRadiusSquared();
        //test from coarsest LOD
        for (auto lod_index = MAX_STATIC_MESH_LODS - 1; lod_index >= 0; --lod_index)
        {
            if (std::pow(lod_data.screen_size_[lod_index] * 0.5f, 2) > screen_size_sqr) {
                return static_cast<int8_t>(eastl::max(lod_index, min_lod));
            }
        }

        return min_lod;
    }

    Coro<void> PostUpdateSystem::UpdateViewVisibility(const ViewInstance& view_instance)
    {
        auto visibility_query = xx;
        const auto job_count = xx;
        const auto view_frustum_permuted = Scene::VolumePermute<Frustum, PermutedFrustum>(xx);

        SmallVector<ECSVisibilityEntities> local_visible_list;
        local_visible_list.resize(job_count);
        co_wait visibility_query.ParallelFor([&]() {
            const auto entity = xx;
            const BoundingBoxSphere boundbox_sphere; //todo
            auto& visibile_list = local_visible_list[xx];
            //instance view relevant check
            if (!CheckInstanceViewRelated()) {
                co_return;
            }
            
            //distance culling 
            const auto view_distance = xx;
            if (!IsAllVisibileInView(, view_distance)) {
                co_return;
            }
            

            //test sphere frustum culling
            if (!IntersectSphereTest(view_frustum_permuted, float4(boundbox_sphere.center_, boundbox_sphere.radius_)) {
                co_return;
            }
            //test boundingbox frustum culling
            if (!IntersectBoxTest(view_frustum_permuted, boundbox_sphere.center_, boundbox_sphere.extent_)) {
                co_return;
            }

            float screen_size = CalcBoundsScreenSize(boundbox_sphere.center_, boundbox_sphere.radius_, ); //todo

            //check the screen-size threshold for visibility
            const auto min_screen_size = GET_PARAM_TYPE_VAL(FLOAT, VIEW_MIN_SCREEN_SIZE);
            if (screen_size < min_screen_size) {
                co_return;
            }

            //compute lod index 
            int8_t lod = CalcTemporalStaticInstanceLOD(); //todo whether calc lod here
            //assign lod to mesh flags

            //append instance to local vibile list
            visibile_list.emplace_back(entity);

        }, job_count);

        
        for (auto& local_list : local_visible_list) {
            //merge local list to global list
            view_instance.visibility_list.merge(local_list);
        }
        co_return;
    }

}
