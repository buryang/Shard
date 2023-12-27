#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include "Graphics/RHI/RHICommonUtils.h"

namespace Shard::RHI {

     struct RHIShaderInitializer
     {
          Renderer::EShaderFrequency     freq_;
          uint32_t     byte_count_{ 0u };
          uint8_t* bytes_{ nullptr };
     };

     class RHIShader {
     public:
          using Ptr = RHIShader*;
          using HashType = Utils::Blake3Hash64;
          RHIShader(const RHIShaderInitializer& initializer) {}
          virtual ~RHIShader() = 0;
          OVERLOAD_OPERATOR_NEW(RHIShader);
     };

     class RHIComputeShader : public RHIShader {
     public:
          using Ptr = RHIComputeShader*;
          RHIComputeShader(const RHIShaderInitializer& initializer) : RHIShader(initializer) {}
          OVERLOAD_OPERATOR_NEW(RHIComputeShader);
     };

     class RHIVertexShader : public RHIShader {
     public:
          using Ptr = RHIVertexShader*;
          RHIVertexShader(const RHIShaderInitializer& initializer) : RHIShader(initializer) {}
          OVERLOAD_OPERATOR_NEW(RHIVertexeShader);
     };

     class RHIHullShader : public RHIShader {
     public:
          using Ptr = RHIHullShader*;
          RHIHullShader(const RHIShaderInitializer& initializer) : RHIShader(initializer) {}
          OVERLOAD_OPERATOR_NEW(RHIHullShader);
     };

     class RHIDomainShader : public RHIShader {
     public:
          using Ptr = RHIDomainShader*;
          RHIDomainShader(const RHIShaderInitializer& initializer) : RHIShader(initializer) {}
          OVERLOAD_OPERATOR_NEW(RHIDomainShader);
     };

     class RHIGeometryShader : public RHIShader {
     public:
          using Ptr = RHIGeometryShader*;
          RHIGeometryShader(const RHIShaderInitializer& initializer) : RHIShader(initializer) {}
          OVERLOAD_OPERATOR_NEW(RHIGeometryShader);
     };

     class RHIPixelShader : public RHIShader {
     public:
          using Ptr = RHIPixelShader*;
          RHIPixelShader(const RHIShaderInitializer& initializer) : RHIShader(initializer) {}
          OVERLOAD_OPERATOR_NEW(RHIPixelShader);
     };

     class RHIRayGenShader : public RHIShader {

     };

     enum
     {
          MAX_RENDER_TARGETS_NUM = 8,
          MAX_VERTEX_DECLARATION_NUM = 32, //my 1060 max input bind/attribute both 32
     };

     struct RHIBlendStateInitializer
     {
          using HashType = Utils::Blake3Hash64;
          struct BlendAttachment
          {
               enum class EBlendOperation
               {
                    eAdd,
                    eSub,
                    eMin,
                    eMax,
                    eNum,
               };
               enum class EBlendFactor
               {
                    eZero,
                    eOne,
                    eSrcColor,
                    eInvSrcColor,
                    eSrcAlpha,
                    eInvSrcAlpha,
                    eDstColor,
                    eInvDstColor,
                    eDstAlpha,
                    eInvDstAlpha,
               };
               enum class EBlendColorMask
               {
                    eNone = 0,
                    eRed = 1 << 0,
                    eGreen = 1 << 1,
                    eBlue = 1 << 2,
                    eAlpha = 1 << 3,

                    eRGB = eRed | eGreen | eBlue,
                    eRGBA = eRGB | eAlpha,
                    //todo
               };
               EBlendFactor     color_src_factor_{};
               EBlendFactor     color_dst_factor_{};
               EBlendOperation     color_blend_op_{};

               EBlendFactor     alpha_src_factor_{};
               EBlendFactor     alpha_dst_factor_{};
               EBlendOperation     alpha_blend_op_{};
               EBlendColorMask     color_write_mask_{};
          };
          Array<BlendAttachment, MAX_RENDER_TARGETS_NUM>     attachments_;
          static HashType ComputeHash(const RHIBlendStateInitializer& initializer);
     };

     struct RHIDepthStencilStateInitializer
     {
          using HashType = Utils::Blake3Hash64;
          enum class EDepthCompareOp
          {
               eNever,
               eLess,
               eEqual,
               eLessOrEqual,
               eGreater,
               eNotEqual,
               eGreaterOrEqual,
               eAlways,
          };
          enum class EStencilOp
          {
               eKeep,
               eZero,
               eReplace,
               eIncrAndClamp,
               eDecrAndClamp,
               eInvert,
               eIncrAndWarp,
               eDecrAndWarp,
          };
          bool     depth_test_enable_{ false };
          bool     depth_write_enable_{ false };
          EDepthCompareOp     depth_compare_op_{ EDepthCompareOp::eNever };
          bool     depth_bound_test_enable_{ false };
          bool     stencil_test_enable_{ false };
          EStencilOp     front_op_;
          EStencilOp     back_op_;
          vec2     depth_bounds_;
          static HashType ComputeHash(const RHIDepthStencilStateInitializer& initializer);
     };

     struct RHIRasterizationStateInitializer
     {
          using HashType = Utils::Blake3Hash64;
          enum class EPolygonMode
          {
               eFill, //d3d solid?
               eLine, //d3d wireframe ?
               ePoint,
          };
          enum class EFrontFace
          {
               eCClk, //counter clock
               eClk,
          };
          enum class ECullingMode
          {
               eNone = 0x0,
               eFront = 0x1,
               eBack = 0x2,
               eAll = eFront | eBack,
          };
          EPolygonMode     poly_mode_{ EPolygonMode::eFill };
          EFrontFace     front_face_{ EFrontFace::eClk };
          ECullingMode     culling_mode_{ ECullingMode::eNone };
          float     depth_bias_{ 0.f };
          static HashType ComputeHash(const RHIRasterizationStateInitializer& initializer);
     };

     struct RHIVertexInputStateInitializer
     {
          using HashType = Utils::Blake3Hash64;
          enum class EInputRate
          {
               eVertex,
               eInstance,
          };
          struct StreamBind {
               uint32_t     binding_{ 0u };
               uint32_t     stride{ 0u };
               EInputRate          input_rate_{ EInputRate::eVertex };
          };
          struct StreamAttribute
          {
               uint32_t     binding_{ 0u };
               uint32_t     stride_{ 0u };
               uint32_t     index_{ 0u };
               uint32_t     offset_{ 0u };
               EPixFormat     format_{ EPixFormat::eUnkown };
          };
          SmallVector<StreamBind, MAX_VERTEX_DECLARATION_NUM>     bindings_;
          SmallVector<StreamAttribute, MAX_VERTEX_DECLARATION_NUM>     declarations_;
          static HashType ComputeHash(const RHIVertexInputStateInitializer& initializer);
     };

     struct RHIPipelineStateObjectInitializer
     {
          using HashType = Utils::Blake3Hash64;
          enum class EType
          {
               eGFX,
               eCompute,
               eRayTrace,
               eNum,
          };
          EType     type_{ EType::eGFX };
          //to do anonymous union default constructor and destructor deleted??
          //union {
               struct GFXInitializer
               {
                    RHIVertexShader::Ptr     vertex_shader_{ nullptr };
                    RHIHullShader::Ptr     hull_shader_{ nullptr };
                    RHIDomainShader::Ptr     domain_shader_{ nullptr };
                    RHIGeometryShader::Ptr     geometry_shader_{ nullptr };
                    RHIPixelShader::Ptr     pixel_shader_{ nullptr };
                    RHIVertexInputStateInitializer     vertex_input_state_;
                    RHIBlendStateInitializer     blend_state_;
                    RHIDepthStencilStateInitializer     depth_stencil_state_;
                    RHIRasterizationStateInitializer     rasterization_state_;
                    //assembly state only has a topology flags
                    EInputTopoType     primitive_topology_{ EInputTopoType::eUnkown };
               }gfx_;
               struct ComputeInitializer
               {
                    RHIComputeShader::Ptr     compute_shader_{ nullptr };
               }compute_;
               struct RayTraceInitializer
               {
                    RHIRayGenShader::Ptr     raygen_shader_{ nullptr };
               }raytrace_;
          //};
          uint32_t     reserved_flags_{ 0u };
          static HashType ComputeHash(const RHIPipelineStateObjectInitializer& initializer);
     };

     class RHIPipelineStateObject {
     public:
          using Ptr = RHIPipelineStateObject*;
          RHIPipelineStateObject(const RHIPipelineStateObjectInitializer& initializer) {};
          OVERLOAD_OPERATOR_NEW(RHIPipelineStateObject);
          virtual ~RHIPipelineStateObject() = 0;
     };

}

