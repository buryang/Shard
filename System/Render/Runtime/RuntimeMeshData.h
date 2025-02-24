#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include "HAL/HALResources.h"

namespace Shard::Render
{
    namespace Runtime
    {

        REGIST_PARAM_TYPE(UINT, RENDER_SYSTEM_BATCH_SIZE_LIMITS, 64u);

        using BatchID = uint32_t;
        using VertexFactoryID = uint32_t;
        using MaterialProxyID = uint32_t;

        struct MeshBatchElement
        {
            uint32_t    offset_{ 0u };
            uint32_t    stride_{ 0u };
            uint32_t    num_primitives_{ 0u };
            uint32_t    num_instances_{ 0u };
            HAL::HALBuffer* buffer_{ nullptr };
        };


        struct MeshBatch
        {
            using SortKey = Utils::SpookyV2Hash32;

            SortKey key_;
            BatchID batch_index_{ 0u };
            SmallVector<MeshBatchElement>   elements_;

            //batch property
            AABB    bound_box_;

            //todo change uint32_t to id
            VertexFactoryID vertex_factory_id_{ ~0u };
            MaterialProxyID material_proxy_id_{ ~0u };
            
            //property
            uint32_t    layer_{ ~0u };
            uint8_t LOD_index_;
            uint32_t    reverse_culling_ : 1;
            uint32_t    backface_culling_enable_ : 1;
            uint32_t    used_for_shadow_ : 1;
            
            //screen size ratio?
            float   min_screen_size_;
            float   max_screen_size_;

            void CalcSortKey() const;

        };

        static constexpr auto g_max_instances = 256u;

        /*batches has the same setting, this to quickly speed up batch filter out no-relavance batches*/
        struct MeshBatchRangeProperty
        {

            BatchID batch_index_{ 0u };
            uint32_t    batch_count_{ 0u };
            uint8_t LOD_index_{ 0u };
            AABB    bound_box_;
            uint32_t    layer_{ 0u }; 
            //todo material/mesh id or hash
            uint32_t    used_for_shadow_ : 1;
            uint32_t    used_for_depth_ : 1;
            uint32_t    with_opaque_material_ : 1;
            uint32_t    with_masked_material_ : 1;
            uint32_t    two_sided_ : 1;

            void AddBatch(const MeshBatch& batch);
            void AddBatches(const Span<MeshBatch>& batches);
        };

        using MeshBatchStream = Vector<MeshBatchElement>;
        using MeshRangeStream = Vector<MeshBatchRangeProperty>;


        //about what to render
        struct RenderFilterSetting
        {
            uint32_t    receive_shadow_ : 1; 
            uint32_t    with_opaque_ : 1;
            uint32_t    with_transparent_ : 1;
            uint32_t    LOD_min_ : 8;
            uint32_t    LOD_range_ : 8;
            uint32_t    layer_mask_{ ~0u };
            /** 
            * \brief
            * \param filter_setting range filter setting
            * \param batch_rangs input batch range to be filter
            * \param batch_rangs_mask  batch range filter result
            */
            static void Filter(const RenderFilterSetting& filter_setting, const Span<MeshBatchRangeProperty>& batch_rangs, BitVector<>& batch_rangs_mask);
        };
    }
}
