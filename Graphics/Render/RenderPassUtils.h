/*****************************************************************//**
 * \file   RenderPassUtils.h
 * \brief  render passs uitls functions
 * 
 * \author buryang
 * \date   December 2024
 *********************************************************************/
#pragma once
#include "RenderGraph.h"
#include "HAL/HALCommand.h"
#include "HAL/HALResources.h"

namespace Shard::Render {

    template<typename ValType>
    void inline AddClearPass(RenderGraph& graph, HAL::HALBuffer* buffer, ValType clear_value)
    {
        auto pass_handle = RenderPass::PassRepoInstance().Alloc();
        graph.AddPass(pass_handle);
    }

    enum class ECopyMode
    {
        eHost2GPU,
        eGPU2Host,
    };

    void inline AddCopyBufferPass(RenderGraph& graph, HAL::HALBuffer* dst_buffer, HAL::HALBuffer* src_buffer, ECopyMode mode)
    {
        auto pass_handle = RenderPass::PassRepoInstance().Alloc();//todo
        graph.AddPass(pass_handle);   }

    void inline AddCopyTexturePass(RenderGraph& graph, HAL::HALTexture* dst_buffer, HAL::HALTexture* src_buffer, ECopyMode mode)
    {
        auto pass_handle = RenderPass::PassRepoInstance().Alloc();//todo
        graph.AddPass(pass_handle);
    }
}
