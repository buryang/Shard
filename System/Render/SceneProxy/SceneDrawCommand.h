#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include "Utils/Memory.h"
#include "Graphics/HAL/HALCommandIR.h"
#include "Graphics/HAL/HALShader.h"

namespace Shard::Renderer
{

    struct MeshElement
    {
        uint32_t primitive_id_;
        uint32_t material_id_;
    };

    static constexpr auto max_mesh_batch_elements = 64u;

    struct MeshDrawBatch
    {

        SmallVector<MeshElement, max_mesh_batch_elements>    elements_;
        //
        void Init();
    };

    /**
     * \brief class  to manage command merge/cache/compile
     */
    class SceneDrawCommandManager final
    {
    public:
        static void SortDrawCommands(Span<MeshDrawCommand>& draw_commands);
  
        SceneDrawCommandManager() = default;
    protected:
        Vector<MeshDrawBatch, > mesh_draw_batches_;
        //ir draw command cache
    };

}
