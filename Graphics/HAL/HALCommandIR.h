#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include "Render/RenderShaderParameters.h"
#include "HAL/HALSync.h"
#include "HAL/HALResources.h"
#include "HAL/HALShader.h"
#include "HAL/HALQuery.h"

namespace Shard::HAL {

    enum class ECommandType :uint32_t {
        eNone,
        eBindStreamSource,
        eSetPrimitiveTopology,
        eSetViewPoint,
        eSetScissorRect,
        eSetPipelineState,
        eSetShader,
        eBindPSO,
        eBeginPass,
        eEndPass,
        eNextSubPass,
        eDraw,
        eDrawIndirect,
        eDrawIndexed,
        eDrawIndexedIndirect,
        eDispatch,
        eDispatchIndirect,
        //transition and barrier
        eBarrier,
        eSplitBarrierBegin,
        eSplitBarrierEnd,
        eCopyBuffer,
        eCopyTexture,
        eCopyBufferTexture,
        eClearTexture,
        eFillBuffer,
        eUpdateBuffer,
        //todo deal with descriptor set update
        eSetTexture,
        eSetSRV,
        eSetUAV,
        eSetBuffer,
        eSetUniformBuffer,
        eSetRawParameter,
        eMSAAResolve,
        ePushConstant,
        //query
        eResetQueryPool,
        eBeginQuery,
        eEndQuery,
        eResolveTimings,
        //shader
        eUpdateShaderTbl,
        //hwrt
        eBuildBLAS,
        eBuildTLAS,
    };

    enum class EContextQueueType {
        eUnkown = 0 << 0,
        eGFX    = 1 << 0,
        eAsyncCompute   = 1 << 1,
        eTransfer   = 1 << 2, //fixme whether we need a transfer context
        eAll = eGFX | eAsyncCompute,
    };

    struct HALCommandPacket
    {
        ECommandType type_{ ECommandType::eNone };
    };

    template<ECommandType TYPE, EContextQueueType QUEUE=EContextQueueType::eGFX>
    struct THALCommandPacketTyped : HALCommandPacket
    {
        static constexpr auto cmd_type   = TYPE;
        static constexpr auto cmd_queue  = QUEUE;

        THALCommandPacketTyped() :type_(cmd_type) {}
    };

#define IMPLEMENT_TYPE(type) ECommandType GetType()const override { return (type); }

    /*for d3d SetStreamSource and vulkan vkCmdBindVertexBuffers*/
    struct HALBindStreamSourcePacket final : public THALCommandPacketTyped<ECommandType::eBindStreamSource> {
        uint32_t    first_stream_index_{ 0u };
        uint32_t    stream_count_{ 0u };
        //uint32_t    stride_;
        HALBuffer**  stream_data_;
        uint64_t*   bytes_offset_;
    };
    
    enum class EPrimitiveTopology
    {
        ePointList,
        eLineList,
        eLineStrip,
        eTriangleList,
        eTriangleStrip,
    };

    struct HALSetPrimitiveTopology final : public THALCommandPacketTyped<ECommandType::eSetPrimitiveTopology> {
        EPrimitiveTopology  topology_{ EPrimitiveTopology::eTriangleStrip };
    };

    struct HALSetViewPointPacket final : public THALCommandPacketTyped<ECommandType::eSetViewPoint> {
        vec3    min_dims_;
        vec3    max_dims_;
    };

    struct HALSetScissorPacket final : public THALCommandPacketTyped<ECommandType::eSetScissorRect> {
        vec4    scissor_;
    };

    struct HALSetPipelineStatePacket final : public THALCommandPacketTyped<ECommandType::eSetPipelineState> {
    };

    struct HALSetShaderPacket final : public THALCommandPacketTyped<ECommandType::eSetShader>{
        HALShader* shader_{ nullptr };
        Render::EShaderFrequency    freq_{};
    };

    struct HALBindPSOPacket final : public THALCommandPacketTyped<ECommandType::eBindPSO> {
        enum EBindPoint {
            eGFX,
            eAsync,
            eRayTrace
        };
        HALPipelineStateObject* pso_{ nullptr };
        EBindPoint    bind_point_{ EBindPoint::eGFX };
    };

    enum class EAttachLoadOpType
    {
        eDiscard,
        eLoad,
        eClear,
        eNotCare,
    };

    enum class EAttachStoreOpType
    {
        eStore,
        eResolve,
        eNotCare,
    };

    struct HALFrameBufferAttachment
    {
        HALTexture* texture_{ nullptr };
        EAttachLoadOpType   load_op_;
        EAttachStoreOpType  store_op_;
        EAccessFlags    src_access_;
        EAccessFlags    dst_access_;
        PixClearValue       clear_val_{ CLEAR_VALUE_BLACK };
        TextureSubRangeRegion   region_;
        EPixFormat  format_{ EPixFormat::eUnkown };
    };

    static constexpr uint32_t MAX_RENDER_TARGET_ATTACHMENTS = 8u;
    struct HALBeginRenderPassPacket final : public THALCommandPacketTyped<ECommandType::eBeginPass, EContextQueueType::eAll>{
        vec4    roi_{ 0u }; //offset.x offset.y wxh
        uint8_t color_attach_count_{ 0u };
        bool    dsv_attach_exist_{ false };
        HALFrameBufferAttachment depth_stencil_;
        HALFrameBufferAttachment color_attachment_[MAX_RENDER_TARGET_ATTACHMENTS];
    };

    struct HALEndRenderPassPacket final : public THALCommandPacketTyped<ECommandType::eEndPass, EContextQueueType::eAll>{
    };

    struct HALNextSubPassPacket final : public THALCommandPacketTyped<ECommandType::eNextSubPass>{
        uint32_t    reserved_flags_{ 0u };

    };

    struct HALDrawPacket final : public THALCommandPacketTyped<ECommandType::eDraw>{
        HALDrawPacket() = default;
        HALDrawPacket(uint32_t num_vertex, uint32_t num_instance, 
                        uint32_t first_vertex, uint32_t first_instance): 
                        num_vertex_(num_vertex), num_instance_(num_instance),
                        first_vertex_(first_vertex), first_instance_(first_instance){}
        uint32_t    num_vertex_{ 0u };
        uint32_t    num_instance_{ 0u };
        uint32_t    first_vertex_{ 0u };
        uint32_t    first_instance_{ 0u }
    };

    struct HALDrawIndexedPacket final : public THALCommandPacketTyped<ECommandType::eDrawIndexed>{
        uint32_t base_vertex_{ 0u };
        uint32_t first_index_{ 0u };
        uint32_t first_instance_{ 0u };
        uint32_t num_vertex_{ 0u }; //num of index too
        uint32_t num_instance_{ 0u };
    };

    struct HALDrawIndexedIndirectPacket final : public THALCommandPacketTyped<ECommandType::eDrawIndexedIndirect>{
        uint32_t draw_count_{ 0 };
        uint32_t first_argument_{ 0 };
        uint32_t stride_{ 0 };
    };

    struct HALDispatchPacket final : public THALCommandPacketTyped<ECommandType::eDispatch, EContextQueueType::eAll>{
        HALShader*  shader_{ nullptr };
        vec3    thread_grp_size_;
    };

    struct HALDispatchIndirectPacket final : public THALCommandPacketTyped<ECommandType::eDispatchIndirect, EContextQueueType::eAll>{
        HALShader*  shader_{ nullptr };
        HALBuffer*  args_buffer_{ nullptr };
        uint32_t    args_offset_{ 0u };
    };

    //d3d use begin_only|end_only flags while vulkan use event to realize split barrier
    struct HALSplitBarrierBeginPacket final : public THALCommandPacketTyped<ECommandType::eSplitBarrierBegin, EContextQueueType::eAll>{
        HALTransitionInfo   trans_info_;
    };

    struct HALSplitBarrierEndPacket final : public THALCommandPacketTyped<ECommandType::eSplitBarrierEnd, EContextQueueType::eAll>{
        HALTransitionInfo   trans_info_;
        //todo split barrier
    };

    struct HALBarrierPacket final : public THALCommandPacketTyped<ECommandType::eBarrier, EContextQueueType::eAll>{
        HALTransitionInfo   trans_info_;
    };


    struct HALCopyBufferPacket final : public THALCommandPacketTyped<ECommandType::eCopyBuffer>{
        HALBuffer*  src_buffer_{ nullptr };
        size_t  src_offset_{ 0 };
        HALBuffer*  dst_buffer_{ nullptr };
        size_t  dst_offset_{ 0 };
        size_t  size_{ 0 };
    };

    struct HALCopyTexturePacket final : public THALCommandPacketTyped<ECommandType::eCopyTexture>{
        HALTexture* src_texture_{ nullptr };
        HALTexture* dst_texture_{ nullptr };
        uint32_t    src_mip_{ 0 };
        uint32_t    dst_mip_{ 0 };
        uint32_t    mip_count_{ 0 };
        uint32_t    xxx_{0};//todo FIXME
    };

    struct HALCopyBufferTexturePacket final : public THALCommandPacketTyped<ECommandType::eCopyBufferTexture>{
        enum class EType : uint8_t {
            eBufferToTexture,
            eTextureToBuffer,
        };
        EType   type_;
        HALBuffer*  buffer_{ nullptr };
        HALTexture* texture_{ nullptr };
        Render::TextureSubRangeRegion   sub_range_;
    };

    struct HALClearBufferPacket final : public THALCommandPacketTyped<ECommandType::eClearTexture>{
        HALTexture* texture_{ nullptr };
        union {
            struct {
                union {
                    vec4    float32_{ 0u };
                    ivec4   int32_;
                    uvec4   uint32_;
                };
            }color_;
            struct {
                float depth_;
                uint32_t stentcil_;
            }depth_stencil_;
        };
        Render::TextureSubRangeRegion   sub_range_;
    };

    struct HALFillBufferPacket final : public THALCommandPacketTyped<ECommandType::eFillBuffer>{
        HALBuffer*  buffer_{ nullptr };
        size_t  offset_{ 0 };
        size_t  size_{ 0 };
        uint32_t    data_;
    };

    struct HALSetBufferPacket final : public THALCommandPacketTyped<ECommandType::eSetBuffer> {

    };

    struct HALMSAAResolvePacket final : public THALCommandPacketTyped<ECommandType::eMSAAResolve>{
        HALTexture*    src_texture_{ nullptr };
        HALTexture*    dst_texture_{ nullptr };

    };

    struct HALPushConstantPacket final : public THALCommandPacketTyped<ECommandType::ePushConstant>{
        HALPushConstantPacket() = default;
        HALPushConstantPacket(uint32_t flags, uint32_t offset, uint8_t* data, size_type size) :
                                flags_(flags), offset_(offset), user_data_(data), user_data_size_(size) {}
        uint32_t    flags_{ 0 };
        uint32_t    offset_{ 0 };
        uint8_t*    user_data_{ nullptr };
        size_type    user_data_size_{ 0u };
    };

#ifdef DEVELOP_DEBUG_TOOLS
    struct HALResetQueryPoolPacket final : public THALCommandPacketTyped<ECommandType::eResetQueryPool> {
        HALQueryPool*   query_pool_{ nullptr };
        uint32_t    first_query_{ 0u };
        uint32_t    query_count_{ 0u };
    };

    struct HALBeginQueryPacket final : public THALCommandPacketTyped<ECommandType::eBeginQuery> {
        //query_pool should not be a timestamp pool
        HALQueryPool*   query_pool_{ nullptr };
        uint32_t    query_index_{ 0u };
    };

    struct HALEndQueryPacket final : public THALCommandPacketTyped<ECommandType::eEndQuery> {
        HALQueryPool*   query_pool_{ nullptr };
        uint32_t    query_index_{ 0u };
    };
#endif
 
}